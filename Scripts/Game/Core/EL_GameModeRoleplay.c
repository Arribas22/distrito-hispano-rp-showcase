[EntityEditorProps(category: "EveronLife/Core", description: "Core gamemode")]
class EL_GameModeRoleplayClass: SCR_BaseGameModeClass
{
}

class EL_GameModeRoleplay: SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		// Initialize turbo boost manager on ALL machines (clients AND listen servers)
		// This is needed because listen servers also need input handling
		bool isServer = Replication.IsServer();
		bool isClient = !isServer || System.IsCLIParam("listenserver") || !RplSession.Mode();
		
		Print(string.Format("[EL_GameModeRoleplay] EOnInit - IsServer: %1, Mode: %2", isServer, RplSession.Mode()), LogLevel.NORMAL);
		
		// Verificar seguridad del servidor (solo en servidor)
		if (isServer)
		{
			EL_ServerSecurityComponent.VerifyServerHostname(owner);
			EL_BanManager.GetInstance().LoadBansAsync();
			EL_MafiaWhitelistManager.GetInstance().LoadEntriesAsync();
			EL_PoliceWhitelistManager.GetInstance().LoadEntriesAsync();
			EL_EmsWhitelistManager.GetInstance().LoadEntriesAsync();
			EL_PendingKickManager.GetInstance().StartPolling();
			EL_PendingLicenseChangeManager.GetInstance().StartPolling();
			EL_VehicleDestructionHandler.GetInstance().Initialize();
		}
		
		// Initialize TurboBoostManager on any machine that has a local player (client or listen server host)
		EL_TurboBoostManager.GetInstance().Initialize();
		Print("[EL_GameModeRoleplay] Turbo Boost Manager initialized", LogLevel.NORMAL);
		
		// Note: FOR_5 Melee damage bonus is now handled in SCR_CharacterDamageManager.c
	}
	
	//------------------------------------------------------------------------------------------------
	//! FIX: Override to handle null killerEntity (e.g. fall damage) preventing crash in SCR_InstigatorContextData
	override void OnControllableDestroyed(IEntity entity, IEntity killerEntity, notnull Instigator instigator)
	{
		// Determine the actual killer entity (mimics vanilla logic)
		IEntity actualKiller = killerEntity;
		if (!actualKiller)
			actualKiller = instigator.GetInstigatorEntity();
		
		// If no killer OR killer is the victim itself (suicide/fall), SKIP vanilla logic to prevent NULL pointer crash
		if (!actualKiller || actualKiller == entity)
		{
			Print("[EL_GameModeRoleplay] OnControllableDestroyed: Self-inflicted death (Fall/Suicide). Skipping vanilla death logic.", LogLevel.WARNING);
			return; 
		}

		// Guard: vanilla SCR_InstigatorContextData calls SCR_PossessingManagerComponent.GetInstance()
		// which returns null in RP mode (no PossessingManager) → crash in GetCharacterControlType
		if (!SCR_PossessingManagerComponent.GetInstance())
		{
			Print("[EL_GameModeRoleplay] OnControllableDestroyed: No PossessingManager, skipping vanilla death tracking.", LogLevel.WARNING);
			return;
		}

		super.OnControllableDestroyed(entity, killerEntity, instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! FIX: Override OnPlayerKilled to handle self-inflicted deaths (second crash point)
	override void OnPlayerKilled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		// Determine the actual killer entity (mimics vanilla logic)
		IEntity actualKiller = killerEntity;
		if (!actualKiller)
			actualKiller = killer.GetInstigatorEntity();
		
		// If no killer OR killer is the victim itself (suicide/fall), SKIP vanilla logic to prevent NULL pointer crash
		if (!actualKiller || actualKiller == playerEntity)
		{
			Print("[EL_GameModeRoleplay] OnPlayerKilled: Self-inflicted death (Fall/Suicide). Skipping vanilla death logic.", LogLevel.WARNING);
			return; 
		}

		// Guard: vanilla SCR_InstigatorContextData calls SCR_PossessingManagerComponent.GetInstance()
		// which returns null in RP mode (no PossessingManager) → crash in GetCharacterControlType
		if (!SCR_PossessingManagerComponent.GetInstance())
		{
			Print("[EL_GameModeRoleplay] OnPlayerKilled: No PossessingManager, skipping vanilla death tracking.", LogLevel.WARNING);
			return;
		}

		super.OnPlayerKilled(playerId, playerEntity, killerEntity, killer);
	}

	//------------------------------------------------------------------------------------------------
	//! FIX GHOST ENTITIES: Proper disconnect handler that calls engine base for RPL cleanup.
	//!
	//! Previous approach skipped super.OnPlayerDisconnected() to avoid SCR_ReconnectComponent
	//! storing the entity. But this also skipped BaseGameMode.OnPlayerDisconnected() (engine base),
	//! which is needed to notify the RPL system about the disconnecting player. Without it, other
	//! clients never receive the entity removal — leaving a "ghost" entity in their world.
	//!
	//! Additionally, DeleteRplEntity(entity, true) deleted the entity locally BEFORE RPL could
	//! propagate the deletion message to clients. Vanilla uses false.
	//!
	//! FIX: Call super to let the engine base + vanilla flow run. SCR_ReconnectComponent IS
	//! present in GameMode_Roleplay.et so GetInstance() won't crash. After super, if the
	//! reconnect system stored the entity, we Forget + delete it. EPF always creates new
	//! characters on reconnect, so vanilla reconnect must not keep stale entities.
	override protected void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		Print(string.Format("[EL_GameModeRoleplay] OnPlayerDisconnected - Player: %1, Cause: %2, Timeout: %3", playerId, cause, timeout), LogLevel.NORMAL);

		// Capture entity reference BEFORE super (vanilla may null its local copy for reconnect)
		IEntity character = null;
		if (IsMaster())
		{
			character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (character)
			{
				// Eject from vehicle before vanilla flow to avoid hierarchy issues
				ChimeraCharacter chChar = ChimeraCharacter.Cast(character);
				if (chChar)
				{
					CompartmentAccessComponent compartmentAccess = CompartmentAccessComponent.Cast(chChar.FindComponent(CompartmentAccessComponent));
					if (compartmentAccess && compartmentAccess.IsInCompartment())
						compartmentAccess.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.INVALID, false);
				}

				// Pre-save persistence as safety net (EPF save chain in super will also save)
				EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(character);
				if (persistence)
				{
					string persistentId = persistence.GetPersistentId();
					if (!persistentId.IsEmpty() && !persistence.IsTemporaryNonPersistent())
					{
						persistence.PauseTracking();
						persistence.Save();
						Print(string.Format("[EL_GameModeRoleplay] Saved persistence for player %1", playerId), LogLevel.NORMAL);
					}
				}
			}
		}

		// Call vanilla flow: BaseGameMode (engine RPL cleanup) → invokers → components
		// → RespawnSystem.OnPlayerDisconnected_S (EPF save) → DeleteRplEntity(char, false)
		// SCR_ReconnectComponent is in the prefab so GetInstance() is safe.
		// If reconnect is enabled, vanilla may STORE the entity instead of deleting it.
		super.OnPlayerDisconnected(playerId, cause, timeout);

		// Post-super: EPF does NOT support vanilla reconnect (always creates new characters).
		// If SCR_ReconnectComponent stored the entity, we must Forget it and delete it
		// to prevent stale RPL entries that cause JIP errors.
		if (IsMaster())
		{
			SCR_ReconnectComponent reconnect = SCR_ReconnectComponent.GetInstance();
			if (reconnect && character)
			{
				SCR_EReconnectState state = reconnect.GetReconnectState(playerId);
				if (state == SCR_EReconnectState.ENTITY_AVAILABLE)
				{
					Print(string.Format("[EL_GameModeRoleplay] Reconnect stored entity for player %1 — cleaning up (EPF handles reconnect via DB)", playerId), LogLevel.WARNING);
					reconnect.Forget(playerId);
					RplComponent.DeleteRplEntity(character, false);
				}
			}

			Print(string.Format("[EL_GameModeRoleplay] Player %1 disconnected - entity cleaned up", playerId), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_GameModeRoleplay()
	{
		EL_PlayerAccountManager.Reset();
	}
}
