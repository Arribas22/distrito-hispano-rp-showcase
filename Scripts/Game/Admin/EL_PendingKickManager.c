//------------------------------------------------------------------------------------------------
//! Gestor de kicks pendientes desde el panel.
//! Consulta la colección PendingKickEntry cada 30 segundos.
//! Si encuentra una entrada cuyo UID está conectado, lo expulsa y elimina la entrada.
//! Las entradas procesadas o expiradas (>5 min) se eliminan automáticamente.
class EL_PendingKickManager
{
	protected static ref EL_PendingKickManager s_pInstance;
	protected bool m_bLoaded;

	// Consultar cada 30 segundos para kicks más rápidos
	protected static const int POLL_INTERVAL_MS = 30000;
	// Entradas más antiguas de 5 minutos se eliminan sin ejecutar
	protected static const int EXPIRY_SECONDS = 300;

	//------------------------------------------------------------------------------------------------
	static EL_PendingKickManager GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new EL_PendingKickManager();
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

		EDF_DbFindCallbackMultiple<EL_PendingKickEntry> callback(this, "OnKicksLoaded");
		dbContext.FindAllAsync(EL_PendingKickEntry, callback: callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnKicksLoaded(EDF_EDbOperationStatusCode statusCode, array<ref EL_PendingKickEntry> results, Managed context)
	{
		if (statusCode != EDF_EDbOperationStatusCode.SUCCESS || !results || results.Count() == 0)
		{
			SchedulePoll();
			return;
		}

		// Obtener jugadores conectados
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		// Crear mapa UID -> playerId para búsqueda rápida
		map<string, int> connectedPlayers = new map<string, int>();
		foreach (int playerId : playerIds)
		{
			string uid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
			if (uid != "")
				connectedPlayers.Set(uid, playerId);
		}

		int currentTime = System.GetUnixTime();

		foreach (EL_PendingKickEntry entry : results)
		{
			if (!entry || !entry.HasId())
				continue;

			// Eliminar entradas expiradas
			if ((currentTime - entry.m_iTimestamp) > EXPIRY_SECONDS)
			{
				RemoveEntry(entry);
				continue;
			}

			// Si el jugador está conectado, kickearlo
			int playerId = connectedPlayers.Get(entry.m_sPlayerUID);
			if (playerId > 0)
			{
				string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
				Print(string.Format("[EL_PendingKickManager] Kicking player '%1' (UID: %2) — Reason: %3 — By: %4",
					playerName, entry.m_sPlayerUID, entry.m_sReason, entry.m_sKickedBy), LogLevel.WARNING);

				GetGame().GetPlayerManager().KickPlayer(playerId, SCR_PlayerManagerKickReason.KICKED_BY_GM);
			}

			// Eliminar la entrada (procesada o jugador no conectado)
			RemoveEntry(entry);
		}

		SchedulePoll();
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveEntry(EL_PendingKickEntry entry)
	{
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (!persistenceManager) return;

		EDF_DbContext dbContext = persistenceManager.GetDbContext();
		if (!dbContext) return;

		dbContext.RemoveAsync(EL_PendingKickEntry, entry.GetId());
	}
}
