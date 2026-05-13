//------------------------------------------------------------------------------------------------
//! Gestor de whitelist de EMS persistente. Carga la lista completa al iniciar y recarga cada 60s.
//! Las entradas se almacenan en la misma DB (MongoDB en producción, JSON en debug) como EL_EmsWhitelistEntry.
//! La recarga periódica permite que cambios manuales en la DB se reflejen sin reiniciar.
class EL_EmsWhitelistManager
{
	protected static ref EL_EmsWhitelistManager s_pInstance;

	// Cache local: playerUID -> WhitelistEntry
	protected ref map<string, ref EL_EmsWhitelistEntry> m_mEntries = new map<string, ref EL_EmsWhitelistEntry>();
	protected bool m_bLoaded;

	// Recarga periódica cada 5 minutos
	protected static const int RELOAD_INTERVAL_MS = 300000;

	//------------------------------------------------------------------------------------------------
	static EL_EmsWhitelistManager GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new EL_EmsWhitelistManager();
		return s_pInstance;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_pInstance = null;
	}

	//------------------------------------------------------------------------------------------------
	void LoadEntriesAsync()
	{
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
		{
			Print("[EL_EmsWhitelist] PersistenceManager not available, retrying in 1s...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(LoadEntriesAsync, 1000, false);
			return;
		}

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
		{
			Print("[EL_EmsWhitelist] DbContext not available, retrying in 1s...", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(LoadEntriesAsync, 1000, false);
			return;
		}

		EDF_DbFindCallbackMultiple<EL_EmsWhitelistEntry> callback(this, "OnEntriesLoaded");
		dbContext.FindAllAsync(EL_EmsWhitelistEntry, callback: callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnEntriesLoaded(EDF_EDbOperationStatusCode statusCode, array<ref EL_EmsWhitelistEntry> results, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
		{
			Print("[EL_EmsWhitelist] Failed to load entries from DB, starting with empty whitelist", LogLevel.WARNING);
			m_bLoaded = true;
			ScheduleReload();
			return;
		}

		m_mEntries.Clear();
		if (results)
		{
			foreach (EL_EmsWhitelistEntry entry : results)
			{
				if (entry && entry.HasId())
					m_mEntries.Set(entry.GetId(), entry);
			}
		}

		m_bLoaded = true;
		Print(string.Format("[EL_EmsWhitelist] Loaded %1 EMS whitelist entries from database", m_mEntries.Count()), LogLevel.NORMAL);

		ScheduleReload();
	}

	//------------------------------------------------------------------------------------------------
	protected void ScheduleReload()
	{
		GetGame().GetCallqueue().Remove(ReloadEntriesAsync);
		GetGame().GetCallqueue().CallLater(ReloadEntriesAsync, RELOAD_INTERVAL_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	void ReloadEntriesAsync()
	{
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager) { ScheduleReload(); return; }

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext) { ScheduleReload(); return; }

		EDF_DbFindCallbackMultiple<EL_EmsWhitelistEntry> callback(this, "OnEntriesReloaded");
		dbContext.FindAllAsync(EL_EmsWhitelistEntry, callback: callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnEntriesReloaded(EDF_EDbOperationStatusCode statusCode, array<ref EL_EmsWhitelistEntry> results, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
		{
			ScheduleReload();
			return;
		}

		// Guardar UIDs previos para detectar eliminados
		set<string> previousUIDs = new set<string>();
		foreach (string uid, EL_EmsWhitelistEntry oldEntry : m_mEntries)
		{
			previousUIDs.Insert(uid);
		}

		int prevCount = m_mEntries.Count();
		m_mEntries.Clear();
		if (results)
		{
			foreach (EL_EmsWhitelistEntry entry : results)
			{
				if (entry && entry.HasId())
					m_mEntries.Set(entry.GetId(), entry);
			}
		}

		int newCount = m_mEntries.Count();
		if (newCount != prevCount)
			Print(string.Format("[EL_EmsWhitelist] Whitelist reloaded: %1 entries (was %2)", newCount, prevCount), LogLevel.NORMAL);

		// Verificar jugadores conectados que ya no están en whitelist
		foreach (string removedUID : previousUIDs)
		{
			if (m_mEntries.Contains(removedUID))
				continue;

			EnforceWhitelistRemoval(removedUID, EL_EJobType.MEDIC, EL_ELicenseType.MEDIC_ACCESS);
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
			Print(string.Format("[EL_EmsWhitelist] ENFORCED: Removed EMS access from connected player '%1' (UID: %2) — whitelist entry was deleted", playerName, playerUID), LogLevel.WARNING);
			break;
		}
	}

	//------------------------------------------------------------------------------------------------
	bool IsWhitelisted(string playerUID)
	{
		return m_mEntries.Contains(playerUID);
	}

	//------------------------------------------------------------------------------------------------
	EL_EmsWhitelistEntry GetEntry(string playerUID)
	{
		return m_mEntries.Get(playerUID);
	}

	//------------------------------------------------------------------------------------------------
	bool IsLoaded()
	{
		return m_bLoaded;
	}

	//------------------------------------------------------------------------------------------------
	void AddPlayer(string playerUID, string playerName, string addedBy)
	{
		EL_EmsWhitelistEntry entry = EL_EmsWhitelistEntry.Create(playerUID, playerName, addedBy);
		m_mEntries.Set(playerUID, entry);

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager) return;

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext) return;

		dbContext.AddOrUpdateAsync(entry);
		Print(string.Format("[EL_EmsWhitelist] Added '%1' (UID: %2) by '%3'", playerName, playerUID, addedBy), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	void RemovePlayer(string playerUID)
	{
		EL_EmsWhitelistEntry entry = m_mEntries.Get(playerUID);
		if (!entry)
		{
			Print(string.Format("[EL_EmsWhitelist] Player UID '%1' is not in the whitelist", playerUID), LogLevel.WARNING);
			return;
		}

		string playerName = entry.m_sPlayerName;
		m_mEntries.Remove(playerUID);

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager) return;

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext) return;

		EDF_DbOperationStatusOnlyCallback callback(this, "OnWhitelistRemoveComplete");
		dbContext.RemoveAsync(EL_EmsWhitelistEntry, playerUID, callback);
		Print(string.Format("[EL_EmsWhitelist] Removed '%1' (UID: %2) from EMS whitelist", playerName, playerUID), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	void OnWhitelistRemoveComplete(EDF_EDbOperationStatusCode statusCode, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS)
			Print(string.Format("[EL_EmsWhitelist] ERROR: RemoveAsync failed with status %1", statusCode), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	int GetEntryCount()
	{
		return m_mEntries.Count();
	}

	//------------------------------------------------------------------------------------------------
	map<string, ref EL_EmsWhitelistEntry> GetAllEntries()
	{
		return m_mEntries;
	}
}
