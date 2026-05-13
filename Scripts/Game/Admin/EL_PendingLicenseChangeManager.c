//------------------------------------------------------------------------------------------------
//! Gestor de cambios de licencia pendientes desde el panel.
//! Consulta la colección PendingLicenseChangeEntry cada 30 segundos.
//! Si encuentra una entrada cuyo UID está conectado, aplica el grant/revoke en memoria.
//! Las entradas procesadas o expiradas (>10 min) se eliminan automáticamente.
class EL_PendingLicenseChangeManager
{
	protected static ref EL_PendingLicenseChangeManager s_pInstance;

	// Consultar cada 30 segundos
	protected static const int POLL_INTERVAL_MS = 30000;
	// Entradas más antiguas de 10 minutos se eliminan sin ejecutar
	protected static const int EXPIRY_SECONDS = 600;

	//------------------------------------------------------------------------------------------------
	static EL_PendingLicenseChangeManager GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new EL_PendingLicenseChangeManager();
		return s_pInstance;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_pInstance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Iniciar el polling (llamar al iniciar el servidor)
	void StartPolling()
	{
		PollAsync();
	}

	//------------------------------------------------------------------------------------------------
	protected void SchedulePoll()
	{
		GetGame().GetCallqueue().Remove(PollAsync);
		GetGame().GetCallqueue().CallLater(PollAsync, POLL_INTERVAL_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	void PollAsync()
	{
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager)
		{
			GetGame().GetCallqueue().CallLater(PollAsync, 1000, false);
			return;
		}

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext)
		{
			GetGame().GetCallqueue().CallLater(PollAsync, 1000, false);
			return;
		}

		EDF_DbFindCallbackMultiple<EL_PendingLicenseChangeEntry> callback(this, "OnChangesLoaded");
		dbContext.FindAllAsync(EL_PendingLicenseChangeEntry, callback: callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnChangesLoaded(EDF_EDbOperationStatusCode statusCode, array<ref EL_PendingLicenseChangeEntry> results, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS || !results || results.Count() == 0)
		{
			SchedulePoll();
			return;
		}

		// Obtener jugadores conectados
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		// Crear mapa UID -> playerId
		map<string, int> connectedPlayers = new map<string, int>();
		foreach (int playerId : playerIds)
		{
			string uid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
			if (uid != "")
				connectedPlayers.Set(uid, playerId);
		}

		int currentTime = System.GetUnixTime();

		foreach (EL_PendingLicenseChangeEntry entry : results)
		{
			if (!entry || !entry.HasId())
				continue;

			// Eliminar entradas expiradas
			if ((currentTime - entry.m_iTimestamp) > EXPIRY_SECONDS)
			{
				RemoveEntry(entry);
				continue;
			}

			// Si el jugador está conectado, aplicar el cambio
			int playerId = connectedPlayers.Get(entry.m_sPlayerUID);
			if (playerId > 0)
			{
				IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
				if (controlledEntity)
				{
					EL_LicenseManagerComponent licComp = EL_Component<EL_LicenseManagerComponent>.Find(controlledEntity);
					if (licComp)
					{
						string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
						EL_ELicenseType licenseType = entry.m_iLicenseId;

						if (entry.m_sAction == "grant")
						{
							if (!licComp.HasLicense(licenseType))
							{
								licComp.UnlockLicense(licenseType, true);
								Print(string.Format("[EL_PendingLicenseChange] GRANTED license #%1 to '%2' (UID: %3) — By: %4",
									entry.m_iLicenseId, playerName, entry.m_sPlayerUID, entry.m_sChangedBy), LogLevel.WARNING);
							}
						}
						else if (entry.m_sAction == "revoke")
						{
							if (licComp.HasLicense(licenseType))
							{
								licComp.RevokeLicense(licenseType);
								Print(string.Format("[EL_PendingLicenseChange] REVOKED license #%1 from '%2' (UID: %3) — By: %4",
									entry.m_iLicenseId, playerName, entry.m_sPlayerUID, entry.m_sChangedBy), LogLevel.WARNING);
							}
						}
					}
				}

				// Entrada procesada — eliminar
				RemoveEntry(entry);
			}
			else
			{
				// Jugador no conectado — la DB ya fue modificada por el panel,
				// cuando el jugador se conecte cargará los datos actualizados.
				// Eliminar la entrada pendiente.
				RemoveEntry(entry);
			}
		}

		SchedulePoll();
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveEntry(EL_PendingLicenseChangeEntry entry)
	{
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager) return;

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext) return;

		dbContext.RemoveAsync(EL_PendingLicenseChangeEntry, entry.GetId());
	}
}
