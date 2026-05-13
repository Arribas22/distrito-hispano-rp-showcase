//------------------------------------------------------------------------------------------------
//! Gestor de whitelist de mafia persistente. Carga la lista completa al iniciar y recarga cada 60s.
//! Las entradas se almacenan en la misma DB (MongoDB en producción, JSON en debug) como EL_MafiaWhitelistEntry.
//! La recarga periódica permite que cambios manuales en la DB se reflejen sin reiniciar.
class EL_MafiaWhitelistManager
{
	protected static ref EL_MafiaWhitelistManager s_pInstance;

	// Cache local: playerUID -> WhitelistEntry
	protected ref map<string, ref EL_MafiaWhitelistEntry> m_mEntries = new map<string, ref EL_MafiaWhitelistEntry>();
	protected bool m_bLoaded;

	// Recarga periódica cada 5 minutos para sincronizar con cambios manuales en DB
	protected static const int RELOAD_INTERVAL_MS = 300000;

	//------------------------------------------------------------------------------------------------
	static EL_MafiaWhitelistManager GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new EL_MafiaWhitelistManager();
		return s_pInstance;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_pInstance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Cargar todas las entradas de la DB al cache (llamar al iniciar el servidor)
	void LoadEntriesAsync()
	{
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
		{
			Print("[EL_MafiaWhitelist] PersistenceManager not available, retrying in 1s...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(LoadEntriesAsync, 1000, false);
			return;
		}

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
		{
			Print("[EL_MafiaWhitelist] DbContext not available, retrying in 1s...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(LoadEntriesAsync, 1000, false);
			return;
		}

		EDF_DbFindCallbackMultiple<EL_MafiaWhitelistEntry> callback(this, "OnEntriesLoaded");
		dbContext.FindAllAsync(EL_MafiaWhitelistEntry, callback: callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnEntriesLoaded(EDF_EDbOperationStatusCode statusCode, array<ref EL_MafiaWhitelistEntry> results, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
		{
			Print("[EL_MafiaWhitelist] Failed to load entries from DB, starting with empty whitelist", LogLevel.WARNING);
			m_bLoaded = true;
			ScheduleReload();
			return;
		}

		m_mEntries.Clear();
		if (results)
		{
			foreach (EL_MafiaWhitelistEntry entry : results)
			{
				if (entry && entry.HasId())
					m_mEntries.Set(entry.GetId(), entry);
			}
		}

		m_bLoaded = true;
		Print(string.Format("[EL_MafiaWhitelist] Loaded %1 mafia whitelist entries from database", m_mEntries.Count()), LogLevel.NORMAL);

		ScheduleReload();
	}

	//------------------------------------------------------------------------------------------------
	//! Programar la siguiente recarga periódica desde DB
	protected void ScheduleReload()
	{
		GetGame().GetCallqueue().Remove(ReloadEntriesAsync);
		GetGame().GetCallqueue().CallLater(ReloadEntriesAsync, RELOAD_INTERVAL_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Recargar entradas desde DB (sincroniza cambios manuales hechos directamente en MongoDB)
	void ReloadEntriesAsync()
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

		EDF_DbFindCallbackMultiple<EL_MafiaWhitelistEntry> callback(this, "OnEntriesReloaded");
		dbContext.FindAllAsync(EL_MafiaWhitelistEntry, callback: callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnEntriesReloaded(EDF_EDbOperationStatusCode statusCode, array<ref EL_MafiaWhitelistEntry> results, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
		{
			ScheduleReload();
			return;
		}

		// Guardar UIDs previos para detectar eliminados
		set<string> previousUIDs = new set<string>();
		foreach (string uid, EL_MafiaWhitelistEntry oldEntry : m_mEntries)
		{
			previousUIDs.Insert(uid);
		}

		int prevCount = m_mEntries.Count();
		m_mEntries.Clear();
		if (results)
		{
			foreach (EL_MafiaWhitelistEntry entry : results)
			{
				if (entry && entry.HasId())
					m_mEntries.Set(entry.GetId(), entry);
			}
		}

		int newCount = m_mEntries.Count();
		if (newCount != prevCount)
			Print(string.Format("[EL_MafiaWhitelist] Whitelist reloaded from DB: %1 entries (was %2)", newCount, prevCount), LogLevel.NORMAL);

		// Verificar jugadores conectados que ya no están en whitelist
		foreach (string removedUID : previousUIDs)
		{
			if (m_mEntries.Contains(removedUID))
				continue;

			EnforceWhitelistRemoval(removedUID, EL_EJobType.MAFIA, EL_ELicenseType.MAFIA_ACCESS);
		}

		ScheduleReload();
	}

	//------------------------------------------------------------------------------------------------
	//! Quitar trabajo y licencia a un jugador conectado que fue eliminado de la whitelist
	protected void EnforceWhitelistRemoval(string playerUID, EL_EJobType jobType, EL_ELicenseType licenseType)
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			string uid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
			if (uid != playerUID)
				continue;

			IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!controlledEntity)
				continue;

			EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(controlledEntity.FindComponent(EL_PlayerJobComponent));
			if (!jobComp || jobComp.GetJob() != jobType)
				continue;

			EL_LicenseManagerComponent licComp = EL_Component<EL_LicenseManagerComponent>.Find(controlledEntity);
			if (licComp && licComp.HasLicense(licenseType))
				licComp.RevokeLicense(licenseType);

			jobComp.AdminSetJob(EL_EJobType.UNEMPLOYED);

			string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
			Print(string.Format("[EL_MafiaWhitelist] ENFORCED: Removed mafia access from connected player '%1' (UID: %2) — whitelist entry was deleted", playerName, playerUID), LogLevel.WARNING);
			break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Verificar si un UID está en la whitelist de mafia (check local, no async)
	bool IsWhitelisted(string playerUID)
	{
		return m_mEntries.Contains(playerUID);
	}

	//------------------------------------------------------------------------------------------------
	//! Obtener entrada de whitelist (null si no está)
	EL_MafiaWhitelistEntry GetEntry(string playerUID)
	{
		return m_mEntries.Get(playerUID);
	}

	//------------------------------------------------------------------------------------------------
	//! ¿Ya se cargó la whitelist?
	bool IsLoaded()
	{
		return m_bLoaded;
	}

	//------------------------------------------------------------------------------------------------
	//! Añadir un jugador a la whitelist: añade al cache local y persiste en DB
	void AddPlayer(string playerUID, string playerName, string addedBy)
	{
		EL_MafiaWhitelistEntry entry = EL_MafiaWhitelistEntry.Create(playerUID, playerName, addedBy);
		m_mEntries.Set(playerUID, entry);

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
			return;

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
			return;

		dbContext.AddOrUpdateAsync(entry);
		Print(string.Format("[EL_MafiaWhitelist] Added '%1' (UID: %2) to mafia whitelist by '%3'", playerName, playerUID, addedBy), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Quitar un jugador de la whitelist: elimina del cache y de la DB
	void RemovePlayer(string playerUID)
	{
		EL_MafiaWhitelistEntry entry = m_mEntries.Get(playerUID);
		if (!entry)
		{
			Print(string.Format("[EL_MafiaWhitelist] Player UID '%1' is not in the whitelist", playerUID), LogLevel.WARNING);
			return;
		}

		string playerName = entry.m_sPlayerName;
		m_mEntries.Remove(playerUID);

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
			return;

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
			return;

		EDF_DbOperationStatusOnlyCallback callback(this, "OnWhitelistRemoveComplete");
		dbContext.RemoveAsync(EL_MafiaWhitelistEntry, playerUID, callback);
		Print(string.Format("[EL_MafiaWhitelist] Removed '%1' (UID: %2) from mafia whitelist", playerName, playerUID), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	void OnWhitelistRemoveComplete(EDF_EDbOperationStatusCode statusCode, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
			Print(string.Format("[EL_MafiaWhitelist] ERROR: RemoveAsync failed with status %1 - entry may still exist in DB!", statusCode), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	int GetEntryCount()
	{
		return m_mEntries.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Obtener el mapa completo de entradas (para serialización RPC)
	map<string, ref EL_MafiaWhitelistEntry> GetAllEntries()
	{
		return m_mEntries;
	}
}
