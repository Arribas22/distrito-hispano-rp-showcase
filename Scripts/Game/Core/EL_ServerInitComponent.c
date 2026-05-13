//------------------------------------------------------------------------------------------------
//! Componente de inicialización del servidor
//! Se ejecuta solo en el servidor cuando arranca
//! Maneja la inicialización de sistemas RPC y limpieza de recursos
class EL_ServerInitComponentClass : SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_ServerInitComponent : SCR_BaseGameModeComponent
{
	protected static EL_ServerInitComponent s_Instance;
	
	// Variables para el cleanup de personajes temporales
	protected int m_iCleanedCount;
	protected ResourceName m_TemporaryPrefab;
	
	// Variables para el cleanup por jugador
	protected string m_sCurrentPlayerUID;
	protected string m_sCurrentCharacterId;
	
	//------------------------------------------------------------------------------------------------
	//! Obtener instancia singleton
	static EL_ServerInitComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
			{
				s_Instance = EL_ServerInitComponent.Cast(gameMode.FindComponent(EL_ServerInitComponent));
			}
		}
		
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Llamado cuando el componente se inicializa en el GameMode
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		// Solo ejecutar en el servidor
		if (!Replication.IsServer())
			return;
		
		//Print("[EL_ServerInitComponent] ==========================================", LogLevel.NORMAL);
		//Print("[EL_ServerInitComponent] SERVER INITIALIZATION STARTED", LogLevel.NORMAL);
		//Print("[EL_ServerInitComponent] ==========================================", LogLevel.NORMAL);
		
		// Inicializar sistemas del servidor
		InitializeServerSystems();

		// Limpiar personajes temporales huÃ©rfanos de sesiones anteriores
		CleanupOrphanedTemporaryCharacters();
		
		// Diferir la protección de vehículos del GarbageSystem para dar tiempo al EPF
		// a restaurar todas las entidades del mundo antes de escanear.
		// 30 segundos es suficiente margen para que EPF termine de cargar.
		GetGame().GetCallqueue().CallLater(ProtectRestoredPlayerVehicles, 30000, false);
		
		//Print("[EL_ServerInitComponent] ==========================================", LogLevel.NORMAL);
		//Print("[EL_ServerInitComponent] SERVER INITIALIZATION COMPLETED", LogLevel.NORMAL);
		//Print("[EL_ServerInitComponent] ==========================================", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Inicializar sistemas del servidor
	protected void InitializeServerSystems()
	{
		//Print("[EL_ServerInitComponent] Initializing server systems...", LogLevel.NORMAL);
		
		// Verificar que EPF estÃ© disponible
		if (!EPF_PersistenceManager.GetInstance())
		{
			Print("[EL_ServerInitComponent] ERROR: EPF Persistence Manager not found!", LogLevel.ERROR);
			return;
		}
		
		//Print("[EL_ServerInitComponent] âœ“ EPF Persistence Manager: OK", LogLevel.NORMAL);
		
		// Verificar que el sistema de cuentas estÃ© disponible
		if (!EL_PlayerAccountManager.GetInstance())
		{
			Print("[EL_ServerInitComponent] ERROR: Player Account Manager not found!", LogLevel.ERROR);
			return;
		}
		
		//Print("[EL_ServerInitComponent] âœ“ Player Account Manager: OK", LogLevel.NORMAL);
		
		// AquÃ­ se pueden inicializar otros sistemas que necesiten arrancar con el servidor
		// Por ejemplo: sistemas de economÃ­a, clima, eventos globales, etc.
		// Verificar que el Lab Manager esté disponible
		if (!EL_LabManagerComponent.GetInstance())
		{
			Print("[EL_ServerInitComponent] WARNING: Lab Manager Component not found on GameMode — lab system disabled", LogLevel.WARNING);
		}

		// Verificar que el Lab Production Manager esté disponible
		if (!EL_LabProductionManager.GetInstance())
		{
			Print("[EL_ServerInitComponent] WARNING: Lab Production Manager not found on GameMode — lab production disabled", LogLevel.WARNING);
		}

		// Forzar carga de recetas de laboratorio
		EL_LabRecipeDatabase.GetInstance();	}
	
	//------------------------------------------------------------------------------------------------
	//! Limpiar personajes temporales huÃ©rfanos de sesiones anteriores
	//! Los personajes temporales se crean cuando un jugador conecta sin cuenta
	//! Deben ser eliminados cuando el jugador crea su primer personaje real
	//! Esta funciÃ³n limpia cualquier temporal que haya quedado huÃ©rfano
	protected void CleanupOrphanedTemporaryCharacters()
	{
		//Print("[EL_ServerInitComponent] Cleaning up orphaned temporary characters...", LogLevel.NORMAL);
		
		// Obtener el mundo del servidor
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Print("[EL_ServerInitComponent] ERROR: World not found!", LogLevel.ERROR);
			return;
		}
		
		// Obtener el SpawnLogic para identificar el prefab temporal
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.Cast(
			GetGame().GetGameMode().FindComponent(SCR_RespawnSystemComponent)
		);
		
		if (!respawnSystem)
		{
			Print("[EL_ServerInitComponent] WARNING: Respawn system not found, skipping temporary character cleanup", LogLevel.WARNING);
			return;
		}
		
		EL_SpawnLogic spawnLogic = EL_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());
		if (!spawnLogic)
		{
			Print("[EL_ServerInitComponent] WARNING: SpawnLogic not found, skipping temporary character cleanup", LogLevel.WARNING);
			return;
		}
		
		ResourceName temporaryPrefab = spawnLogic.GetTemporaryMenuPrefab();
		//Print(string.Format("[EL_ServerInitComponent] Temporary character prefab: %1", temporaryPrefab), LogLevel.NORMAL);
		
		// Guardar prefab temporal para usar en callbacks
		m_TemporaryPrefab = temporaryPrefab;
		m_iCleanedCount = 0;
		
		// Buscar todas las entidades en el mundo usando callbacks
		world.QueryEntitiesBySphere(
			vector.Zero,
			999999, // Radio enorme para cubrir todo el mapa
			QueryOrphanedCharactersCallback,
			FilterTemporaryCharacters,
			EQueryEntitiesFlags.ALL
		);
		
		//Print(string.Format("[EL_ServerInitComponent] âœ“ Cleaned up %1 orphaned temporary character(s)", m_iCleanedCount), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback para procesar cada entidad encontrada
	protected bool QueryOrphanedCharactersCallback(IEntity entity)
	{
		if (!entity)
			return true;
		
		// Verificar si tiene el prefab temporal
		ResourceName entityPrefab = entity.GetPrefabData().GetPrefabName();
		if (entityPrefab != m_TemporaryPrefab)
			return true;
		
		// Verificar si tiene componente de persistencia
		EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(entity);
		if (!persistence)
			return true;
		
		// Verificar si es temporal
		if (persistence.IsTemporaryNonPersistent())
		{
			string persistentId = persistence.GetPersistentId();
			//Print(string.Format("[EL_ServerInitComponent] Deleting orphaned temporary character: %1", persistentId), LogLevel.NORMAL);
			
			// FIX GHOST ENTITIES: Use RPL deletion so clients also remove the entity.
			// SCR_EntityHelper.DeleteEntityAndChildren is LOCAL-only and leaves ghosts on clients.
			RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
			if (rpl)
				RplComponent.DeleteRplEntity(entity, false);
			else
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			m_iCleanedCount++;
		}
		
		return true; // Continuar buscando
	}
	
	//------------------------------------------------------------------------------------------------
	//! Filtro para buscar solo entidades de personajes
	protected static bool FilterTemporaryCharacters(IEntity entity)
	{
		// Solo considerar entidades que tengan componente de personaje
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(
			entity.FindComponent(CharacterControllerComponent)
		);
		
		return controller != null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Limpiar personajes temporales de un jugador especÃ­fico
	//! Llamado cuando un jugador crea su primer personaje real
	static void CleanupTemporaryCharactersForPlayer(string playerUID, int delaySeconds = 0)
	{
		if (!Replication.IsServer())
		{
			Print("[EL_ServerInitComponent] ERROR: CleanupTemporaryCharactersForPlayer called on client!", LogLevel.ERROR);
			return;
		}
		
		EL_ServerInitComponent instance = GetInstance();
		if (!instance)
		{
			Print("[EL_ServerInitComponent] ERROR: Instance not found!", LogLevel.ERROR);
			return;
		}
		
		if (delaySeconds > 0)
		{
			// Ejecutar con delay
			GetGame().GetCallqueue().CallLater(
				instance.CleanupTemporaryCharactersForPlayerInternal,
				delaySeconds * 1000,
				false,
				playerUID
			);
		}
		else
		{
			// Ejecutar inmediatamente
			instance.CleanupTemporaryCharactersForPlayerInternal(playerUID);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! ImplementaciÃ³n interna de limpieza por jugador
	protected void CleanupTemporaryCharactersForPlayerInternal(string playerUID)
	{
		//Print(string.Format("[EL_ServerInitComponent] Cleaning up temporary characters for player: %1", playerUID), LogLevel.NORMAL);
		
		// Obtener el SpawnLogic para identificar el prefab temporal
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.Cast(
			GetGame().GetGameMode().FindComponent(SCR_RespawnSystemComponent)
		);
		
		if (!respawnSystem)
			return;
		
		EL_SpawnLogic spawnLogic = EL_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());
		if (!spawnLogic)
			return;
		
		ResourceName temporaryPrefab = spawnLogic.GetTemporaryMenuPrefab();
		
		// Buscar el playerId del jugador
		int playerId = -1;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			array<int> playerIds = {};
			playerManager.GetPlayers(playerIds);
			
			foreach (int pid : playerIds)
			{
				string uid = EL_Utils.GetPlayerSteamUID(pid);
				if (uid == playerUID)
				{
					playerId = pid;
					break;
				}
			}
		}
		
		if (playerId < 0)
		{
			Print(string.Format("[EL_ServerInitComponent] Player %1 not found online, skipping cleanup", playerUID), LogLevel.WARNING);
			return;
		}
		
		// Obtener la entidad actual del jugador
		IEntity currentEntity = playerManager.GetPlayerControlledEntity(playerId);
		string currentCharacterId = "";
		
		if (currentEntity)
		{
			EPF_PersistenceComponent currentPersistence = EPF_Component<EPF_PersistenceComponent>.Find(currentEntity);
			if (currentPersistence)
			{
				currentCharacterId = currentPersistence.GetPersistentId();
			}
		}
		
		// Buscar todas las entidades del mundo
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		
		// Preparar variables para el callback
		m_TemporaryPrefab = temporaryPrefab;
		m_sCurrentPlayerUID = playerUID;
		m_sCurrentCharacterId = currentCharacterId;
		m_iCleanedCount = 0;
		
		// Buscar entidades usando callbacks
		world.QueryEntitiesBySphere(
			vector.Zero,
			999999,
			QueryPlayerTemporaryCharactersCallback,
			FilterTemporaryCharacters,
			EQueryEntitiesFlags.ALL
		);
		
		//Print(string.Format("[EL_ServerInitComponent] âœ“ Cleaned up %1 temporary character(s) for player %2", m_iCleanedCount, playerUID), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback para procesar personajes temporales de un jugador especÃ­fico
	protected bool QueryPlayerTemporaryCharactersCallback(IEntity entity)
	{
		if (!entity)
			return true;
		
		// No eliminar la entidad actual del jugador, SALVO que sea la entidad LOBBY
		// (la entidad LOBBY siempre debe eliminarse cuando el jugador selecciona su personaje real)
		EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(entity);
		if (!persistence)
			return true;
		
		string entityId = persistence.GetPersistentId();
		bool isLobbyEntity = entityId.EndsWith("_LOBBY");
		
		// Si es el personaje actual Y NO es una entidad LOBBY â†’ saltar (proteger entidad actual)
		if (entityId == m_sCurrentCharacterId && !isLobbyEntity)
			return true; // Es el personaje real actual, no eliminar
		
		// Verificar si es temporal
		if (!persistence.IsTemporaryNonPersistent())
			return true;
		
		// Verificar si tiene el prefab temporal
		ResourceName entityPrefab = entity.GetPrefabData().GetPrefabName();
		if (entityPrefab != m_TemporaryPrefab)
			return true;
		
		// Si la entidad LOBBY sigue siendo la entidad actual, el handover todavÃ­a no terminÃ³.
		// La limpieza se relanza desde OnHandoverComplete cuando el jugador ya controla
		// su entidad definitiva.
		if (isLobbyEntity && entityId == m_sCurrentCharacterId)
		{
			Print(string.Format("[EL_ServerInitComponent] Skipping cleanup for active LOBBY entity until handover completes: %1", entityId), LogLevel.WARNING);
			return true;
		}
		
		//Print(string.Format("[EL_ServerInitComponent] Deleting temporary character: %1 for player %2", entityId, m_sCurrentPlayerUID), LogLevel.NORMAL);
		// FIX GHOST ENTITIES: Use RPL deletion so clients also remove the entity.
		// SCR_EntityHelper.DeleteEntityAndChildren is LOCAL-only and leaves ghosts on clients.
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (rpl)
			RplComponent.DeleteRplEntity(entity, false);
		else
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		m_iCleanedCount++;
		
		return true; // Continuar buscando
	}

	//------------------------------------------------------------------------------------------------
	//! Proteger vehículos de jugadores del GarbageSystem tras el arranque del servidor.
	//! Se llama 30 s después del inicio para dar tiempo al EPF a restaurar entidades del mundo.
	//! Sin esto, el GarbageSystem borra vehículos cuando ningún jugador está cerca,
	//! dejando las llaves huérfanas (el coche desaparece del mundo y del garaje).
	protected int m_iProtectedVehicleCount = 0;

	protected void ProtectRestoredPlayerVehicles()
	{
		if (!Replication.IsServer())
			return;
		
		m_iProtectedVehicleCount = 0;
		
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		
		world.QueryEntitiesBySphere(
			vector.Zero,
			999999,
			ProtectVehicleCallback,
			FilterPlayerVehicles,
			EQueryEntitiesFlags.ALL
		);
		
		Print(string.Format("[EL_ServerInitComponent] GarbageSystem protection applied to %1 restored player vehicle(s)", m_iProtectedVehicleCount), LogLevel.NORMAL);
	}
	
	//! Callback: aplica GarbageSystem.Ignore() a vehículos con dueño
	protected bool ProtectVehicleCallback(IEntity entity)
	{
		if (!entity)
			return true;
		
		EL_VehicleManagerComponent mgr = EL_VehicleManagerComponent.Cast(entity.FindComponent(EL_VehicleManagerComponent));
		if (!mgr)
			return true;
		
		string ownerId = mgr.GetOwnerId();
		if (ownerId.IsEmpty())
			return true;
		
		SCR_GarbageSystem gs = SCR_GarbageSystem.GetByEntityWorld(entity);
		if (gs)
		{
			gs.UpdateBlacklist(entity, true);
			m_iProtectedVehicleCount++;
		}
		
		return true;
	}
	
	//! Filtro: solo entidades con EL_VehicleManagerComponent
	protected static bool FilterPlayerVehicles(IEntity entity)
	{
		return entity.FindComponent(EL_VehicleManagerComponent) != null;
	}

}
