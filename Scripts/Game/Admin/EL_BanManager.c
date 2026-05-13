//------------------------------------------------------------------------------------------------
//! Gestor de bans persistentes. Carga la lista completa al iniciar y recarga cada 60s.
//! Los bans se almacenan en la misma DB (MongoDB en producción, JSON en debug) como EL_BanEntry.
//! La recarga periódica permite que cambios manuales en la DB se reflejen sin reiniciar.
class EL_BanManager
{
	protected static ref EL_BanManager s_pInstance;

	// Cache local: playerUID -> BanEntry
	protected ref map<string, ref EL_BanEntry> m_mBans = new map<string, ref EL_BanEntry>();
	protected bool m_bLoaded;

	// Recarga periódica cada 5 minutos para sincronizar con cambios manuales en DB
	protected static const int RELOAD_INTERVAL_MS = 300000;

	//------------------------------------------------------------------------------------------------
	static EL_BanManager GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new EL_BanManager();
		return s_pInstance;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_pInstance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Cargar todos los bans de la DB al cache (llamar al iniciar el servidor)
	void LoadBansAsync()
	{
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
		{
			Print("[EL_BanManager] PersistenceManager not available, retrying in 1s...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(LoadBansAsync, 1000, false);
			return;
		}

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
		{
			Print("[EL_BanManager] DbContext not available, retrying in 1s...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(LoadBansAsync, 1000, false);
			return;
		}

		EDF_DbFindCallbackMultiple<EL_BanEntry> callback(this, "OnBansLoaded");
		dbContext.FindAllAsync(EL_BanEntry, callback: callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnBansLoaded(EDF_EDbOperationStatusCode statusCode, array<ref EL_BanEntry> results, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
		{
			Print("[EL_BanManager] Failed to load bans from DB, starting with empty ban list", LogLevel.WARNING);
			m_bLoaded = true;
			ScheduleReload();
			return;
		}

		m_mBans.Clear();
		if (results)
		{
			foreach (EL_BanEntry entry : results)
			{
				if (entry && entry.HasId())
					m_mBans.Set(entry.GetId(), entry);
			}
		}

		m_bLoaded = true;
		Print(string.Format("[EL_BanManager] Loaded %1 persistent ban(s) from database", m_mBans.Count()), LogLevel.NORMAL);

		ScheduleReload();
	}

	//------------------------------------------------------------------------------------------------
	//! Programar la siguiente recarga periódica desde DB
	protected void ScheduleReload()
	{
		GetGame().GetCallqueue().Remove(ReloadBansAsync);
		GetGame().GetCallqueue().CallLater(ReloadBansAsync, RELOAD_INTERVAL_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Recargar bans desde DB (sincroniza cambios manuales hechos directamente en MongoDB)
	void ReloadBansAsync()
	{
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
		{
			ScheduleReload();
			return;
		}

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
		{
			ScheduleReload();
			return;
		}

		EDF_DbFindCallbackMultiple<EL_BanEntry> callback(this, "OnBansReloaded");
		dbContext.FindAllAsync(EL_BanEntry, callback: callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnBansReloaded(EDF_EDbOperationStatusCode statusCode, array<ref EL_BanEntry> results, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
		{
			ScheduleReload();
			return;
		}

		int prevCount = m_mBans.Count();
		m_mBans.Clear();
		if (results)
		{
			foreach (EL_BanEntry entry : results)
			{
				if (entry && entry.HasId())
					m_mBans.Set(entry.GetId(), entry);
			}
		}

		int newCount = m_mBans.Count();
		if (newCount != prevCount)
			Print(string.Format("[EL_BanManager] Ban list reloaded from DB: %1 ban(s) (was %2)", newCount, prevCount), LogLevel.NORMAL);

		ScheduleReload();
	}

	//------------------------------------------------------------------------------------------------
	//! Verificar si un UID está baneado (check local, no async)
	bool IsBanned(string playerUID)
	{
		return m_mBans.Contains(playerUID);
	}

	//------------------------------------------------------------------------------------------------
	//! Obtener entrada de ban (null si no está baneado)
	EL_BanEntry GetBanEntry(string playerUID)
	{
		return m_mBans.Get(playerUID);
	}

	//------------------------------------------------------------------------------------------------
	//! ¿Ya se cargó la lista de bans?
	bool IsLoaded()
	{
		return m_bLoaded;
	}

	//------------------------------------------------------------------------------------------------
	//! Banear a un jugador: añade al cache local y persiste en DB
	void BanPlayer(string playerUID, string playerName, string bannedBy, string reason = "Ban de administrador")
	{
		EL_BanEntry entry = EL_BanEntry.Create(playerUID, playerName, bannedBy, reason);
		m_mBans.Set(playerUID, entry);

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
			return;

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
			return;

		dbContext.AddOrUpdateAsync(entry);
		Print(string.Format("[EL_BanManager] Banned player '%1' (UID: %2) by '%3'. Reason: %4", playerName, playerUID, bannedBy, reason), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Desbanear a un jugador: elimina del cache, de la DB y del BanService nativo del motor
	void UnbanPlayer(string playerUID)
	{
		EL_BanEntry entry = m_mBans.Get(playerUID);
		if (!entry)
		{
			Print(string.Format("[EL_BanManager] Player UID '%1' is not banned", playerUID), LogLevel.WARNING);
			return;
		}

		string playerName = entry.m_sPlayerName;
		m_mBans.Remove(playerUID);

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
			return;

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
			return;

		EDF_DbOperationStatusOnlyCallback callback(this, "OnBanRemoveComplete");
		dbContext.RemoveAsync(EL_BanEntry, playerUID, callback);
		Print(string.Format("[EL_BanManager] Unbanned player '%1' (UID: %2)", playerName, playerUID), LogLevel.WARNING);

		// Eliminar también el ban del BanService nativo del motor (creado por BANNED_BY_GM en versiones antiguas)
		BackendApi backendApi = GetGame().GetBackendApi();
		if (backendApi)
		{
			BanServiceApi banService = backendApi.GetBanServiceApi();
			if (banService)
			{
				array<string> ids = new array<string>();
				ids.Insert(playerUID);
				banService.RemoveBans(null, ids);
				Print(string.Format("[EL_BanManager] BanService engine removal requested for '%1' (UID: %2)", playerName, playerUID), LogLevel.NORMAL);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnBanRemoveComplete(EDF_EDbOperationStatusCode statusCode, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
			Print(string.Format("[EL_BanManager] ERROR: RemoveAsync failed with status %1 - ban may still exist in DB!", statusCode), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	int GetBanCount()
	{
		return m_mBans.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Obtener el mapa completo de bans (para serialización RPC)
	map<string, ref EL_BanEntry> GetAllBans()
	{
		return m_mBans;
	}
}
