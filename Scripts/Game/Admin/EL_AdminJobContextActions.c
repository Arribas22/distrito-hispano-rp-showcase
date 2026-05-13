//------------------------------------------------------------------------------------------------
//! Admin Job Context Actions
//! Acciones del menu contextual del administrador (Game Master) para asignar/quitar
//! los trabajos de Policía y EMS a jugadores sin pasar por licencias ni whitelist.
//!
//! Se muestran al hacer click derecho sobre una entidad de tipo CHARACTER en el editor GM.
//! Requieren que la accion este configurada como IsServer=true en el conf/prefab.
//!
//! Patron de uso:
//!   - Hereda SCR_SelectedEntitiesContextAction
//!   - CanBeShown: solo personajes jugadores (no IA)
//!   - CanBePerformed: solo si el trabajo actual es el adecuado (evita opciones sin efecto)
//!   - Perform: llama AdminSetJob en EL_PlayerJobComponent del objetivo
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
// ================== POLICÍA ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Asignar trabajo de POLICÍA a un jugador (bypass de licencia, solo admin)
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminSetPoliceContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		// Solo jugadores reales (no IA)
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return false;

		// Mostrar solo si NO es ya policía
		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return false;

		return jobComp.GetJob() != EL_EJobType.POLICE;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		return (jobComp != null);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return;

		jobComp.AdminSetJob(EL_EJobType.POLICE);

		// Añadir a la whitelist persistente
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		string playerUID  = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		EL_PoliceWhitelistManager policeManager = EL_PoliceWhitelistManager.GetInstance();
		if (policeManager && !policeManager.IsWhitelisted(playerUID))
			policeManager.AddPlayer(playerUID, playerName, "ADMIN");

		Print(string.Format("[EL_AdminJobContextActions] ADMIN puso como POLICÍA a: %1 (UID: %2)", playerName, playerUID), LogLevel.NORMAL);
	}
}


//------------------------------------------------------------------------------------------------
//! Quitar trabajo de POLICÍA a un jugador y revocar la licencia POLICE_ACCESS
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminRemovePoliceContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return false;

		// Mostrar solo si ES policía actualmente
		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return false;

		return jobComp.GetJob() == EL_EJobType.POLICE;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		return (jobComp != null);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return;

		// Cambiar a desempleado (admin override)
		jobComp.AdminSetJob(EL_EJobType.UNEMPLOYED);

		// Revocar la licencia de acceso policial
		EL_LicenseManagerComponent licComp = EL_Component<EL_LicenseManagerComponent>.Find(entity);
		if (licComp && licComp.HasLicense(EL_ELicenseType.POLICE_ACCESS))
			licComp.RevokeLicense(EL_ELicenseType.POLICE_ACCESS);

		// Quitar de la whitelist persistente
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		string playerUID  = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		EL_PoliceWhitelistManager policeManager = EL_PoliceWhitelistManager.GetInstance();
		if (policeManager)
			policeManager.RemovePlayer(playerUID);

		Print(string.Format("[EL_AdminJobContextActions] ADMIN quitó POLICÍA a: %1 (UID: %2)", playerName, playerUID), LogLevel.NORMAL);
	}
}


//------------------------------------------------------------------------------------------------
//! GM: alternar requisito de policías (depuración)
//! Se muestra al hacer click en el suelo (sin entidad seleccionada).
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminTogglePoliceRequirementContextAction : SCR_GeneralContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		// Solo mostrar al hacer click en el suelo (sin entidad hover ni seleccionada)
		if (hoveredEntity || !selectedEntities.IsEmpty())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		bool current = EL_Utils.IsPoliceRequirementIgnored();
		EL_Utils.SetIgnorePoliceRequirement(!current);
		string status;
		if (current)
			status = WidgetManager.Translate("#DH-Admin_PoliceReq_Enabled");
		else
			status = WidgetManager.Translate("#DH-Admin_PoliceReq_Disabled");
		string text = WidgetManager.Translate("#DH-Admin_PoliceReq_ToggledFmt", status);
		Print(string.Format("[EL_AdminJobContextActions] GM toggled police requirement -> %1", text), LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint(text, WidgetManager.Translate("#DH-Admin_GM"), 6.0);
	}
}



//------------------------------------------------------------------------------------------------
// ================== EMS ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Asignar trabajo de EMS (Médico) a un jugador (bypass de licencia, solo admin)
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminSetEMSContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return false;

		// Mostrar solo si NO es ya médico/EMS
		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return false;

		return jobComp.GetJob() != EL_EJobType.MEDIC;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		return (jobComp != null);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return;

		jobComp.AdminSetJob(EL_EJobType.MEDIC);

		// Añadir a la whitelist persistente
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		string playerUID  = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		EL_EmsWhitelistManager emsManager = EL_EmsWhitelistManager.GetInstance();
		if (emsManager && !emsManager.IsWhitelisted(playerUID))
			emsManager.AddPlayer(playerUID, playerName, "ADMIN");

		Print(string.Format("[EL_AdminJobContextActions] ADMIN puso como EMS a: %1 (UID: %2)", playerName, playerUID), LogLevel.NORMAL);
	}
}


//------------------------------------------------------------------------------------------------
//! Quitar trabajo de EMS (Médico) a un jugador y revocar la licencia MEDIC_ACCESS
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminRemoveEMSContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return false;

		// Mostrar solo si ES médico actualmente
		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return false;

		return jobComp.GetJob() == EL_EJobType.MEDIC;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		return (jobComp != null);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return;

		// Cambiar a desempleado (admin override)
		jobComp.AdminSetJob(EL_EJobType.UNEMPLOYED);

		// Revocar la licencia de acceso EMS
		EL_LicenseManagerComponent licComp = EL_Component<EL_LicenseManagerComponent>.Find(entity);
		if (licComp && licComp.HasLicense(EL_ELicenseType.MEDIC_ACCESS))
			licComp.RevokeLicense(EL_ELicenseType.MEDIC_ACCESS);

		// Quitar de la whitelist persistente
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		string playerUID  = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		EL_EmsWhitelistManager emsManager = EL_EmsWhitelistManager.GetInstance();
		if (emsManager)
			emsManager.RemovePlayer(playerUID);

		Print(string.Format("[EL_AdminJobContextActions] ADMIN quitó EMS a: %1 (UID: %2)", playerName, playerUID), LogLevel.NORMAL);
	}
}


//------------------------------------------------------------------------------------------------
// ================== WHITELIST POLICÍA / EMS MENU ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Abre el menú combinado de gestión de whitelist de Policía y EMS. Siempre visible para el GM.
//! Se muestra al hacer click en el suelo (sin entidad seleccionada).
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminOpenPoliceEmsWhitelistContextAction : SCR_GeneralContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		if (hoveredEntity || !selectedEntities.IsEmpty())
			return false;
		return true;
	}

	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		return true;
	}

	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
			menuManager.OpenMenu(ChimeraMenuPreset.EL_PoliceEmsWhitelistMenu);
	}
}


//------------------------------------------------------------------------------------------------
// ================== MAFIA ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Asignar trabajo de MAFIA a un jugador (bypass de licencia, solo admin)
//! También lo añade a la whitelist de mafia para que mantenga acceso entre sesiones.
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminSetMafiaContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return false;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return false;

		return jobComp.GetJob() != EL_EJobType.MAFIA;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		return (jobComp != null);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return;

		jobComp.AdminSetJob(EL_EJobType.MAFIA);

		// Añadir a la whitelist de mafia para persistencia entre sesiones
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		string playerUID = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		
		EL_MafiaWhitelistManager mafiaManager = EL_MafiaWhitelistManager.GetInstance();
		if (mafiaManager && !mafiaManager.IsWhitelisted(playerUID))
			mafiaManager.AddPlayer(playerUID, playerName, "ADMIN");

		Print(string.Format("[EL_AdminJobContextActions] ADMIN puso como MAFIA a: %1 (UID: %2)", playerName, playerUID), LogLevel.NORMAL);
	}
}


//------------------------------------------------------------------------------------------------
//! Quitar trabajo de MAFIA a un jugador, revocar MAFIA_ACCESS y eliminar de whitelist
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminRemoveMafiaContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return false;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return false;

		return jobComp.GetJob() == EL_EJobType.MAFIA;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		return (jobComp != null);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(entity.FindComponent(EL_PlayerJobComponent));
		if (!jobComp)
			return;

		jobComp.AdminSetJob(EL_EJobType.UNEMPLOYED);

		// Revocar la licencia de acceso mafia
		EL_LicenseManagerComponent licComp = EL_Component<EL_LicenseManagerComponent>.Find(entity);
		if (licComp && licComp.HasLicense(EL_ELicenseType.MAFIA_ACCESS))
			licComp.RevokeLicense(EL_ELicenseType.MAFIA_ACCESS);

		// Eliminar de la whitelist de mafia
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		string playerUID = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		
		EL_MafiaWhitelistManager mafiaManager = EL_MafiaWhitelistManager.GetInstance();
		if (mafiaManager)
			mafiaManager.RemovePlayer(playerUID);

		Print(string.Format("[EL_AdminJobContextActions] ADMIN quitó MAFIA a: %1 (UID: %2)", playerName, playerUID), LogLevel.NORMAL);
	}
}


//------------------------------------------------------------------------------------------------
// ================== WARNING (AVISO ADMIN) ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Enviar un aviso administrativo al jugador con sirena (notificación custom)
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminWarningPlayerContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		return playerId > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		return CanBeShown(selectedEntity, cursorWorldPosition, flags);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		// Enviar notificación con sirena al jugador
		EL_NotificationManagerComponent.NotifyPlayer(
			playerId,
			WidgetManager.Translate("#DH-Admin_Warning_Title"),
			WidgetManager.Translate("#DH-Admin_Warning_Message"),
			15.0,
			EL_ENotificationType.POLICE_ALERT
		);

		Print(string.Format("[EL_AdminJobContextActions] ADMIN envió WARNING a: %1 (ID: %2)", playerName, playerId), LogLevel.WARNING);
	}
}


//------------------------------------------------------------------------------------------------
// ================== KICK ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Expulsar a un jugador del servidor (kick temporal)
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminKickPlayerContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		// Solo jugadores reales (no IA)
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		return playerId > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		return CanBeShown(selectedEntity, cursorWorldPosition, flags);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		Print(string.Format("[EL_AdminJobContextActions] ADMIN expulsó (kick) a: %1 (ID: %2)", playerName, playerId), LogLevel.NORMAL);

		// Enviar aviso al jugador antes de expulsarle
		PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (controller)
		{
			SCR_HintManagerComponent.ShowCustomHint(
				WidgetManager.Translate("#DH-Admin_Kick_Message"),
				WidgetManager.Translate("#DH-Admin_Kick_Title"),
				5.0
			);
		}

		// Kick usando la causa específica de GM del motor
		GetGame().GetPlayerManager().KickPlayer(playerId, SCR_PlayerManagerKickReason.KICKED_BY_GM);
	}
}


//------------------------------------------------------------------------------------------------
// ================== BAN ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Banear (expulsión permanente de sesión) a un jugador — kick con causa BAN
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminBanPlayerContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		return playerId > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		return CanBeShown(selectedEntity, cursorWorldPosition, flags);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);

		// Persistir ban en la base de datos
		EL_BanManager.GetInstance().BanPlayer(playerUID, playerName, "GM");

		// Notificar al jugador antes de banearlo
		PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (controller)
		{
			SCR_HintManagerComponent.ShowCustomHint(
				WidgetManager.Translate("#DH-Admin_Ban_Message"),
				WidgetManager.Translate("#DH-Admin_Ban_Title"),
				5.0
			);
		}

		// Kickear al jugador SIN crear ban nativo del motor (el ban lo gestiona EL_BanManager + MongoDB).
		// BANNED_BY_GM crea un ban en el BanService nativo de Arma que NO se puede deshacer desde script,
		// lo que hace imposible desbanear. Usamos KICKED_BY_GM para que solo nuestro sistema gestione el ban.
		GetGame().GetPlayerManager().KickPlayer(playerId, SCR_PlayerManagerKickReason.KICKED_BY_GM);
	}
}


//------------------------------------------------------------------------------------------------
// ================== CURACIÓN ADMIN ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Dar 1000 de XP a un jugador (solo admin)
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminAddXPContextAction : SCR_SelectedEntitiesContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags) { return true; }
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags) { return true; }

	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity) return;

		// Intentar buscar el componente en el personaje o en el controlador
		EL_PlayerLevelComponent levelComp = EL_PlayerLevelComponent.Cast(entity.FindComponent(EL_PlayerLevelComponent));
		
		if (!levelComp)
		{
			// Si no esta en el personaje, buscar en el PlayerController
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
			if (pc) levelComp = EL_PlayerLevelComponent.Cast(pc.FindComponent(EL_PlayerLevelComponent));
		}

		if (levelComp)
		{
			levelComp.AddExperience(1000.0, "ADMIN");
			Print("[EL_AdminJobContextActions] ADMIN dio 1000 XP", LogLevel.NORMAL);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Curar completamente a un jugador: salud, hambre y sed al 100%
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminHealPlayerContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		return playerId > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		return CanBeShown(selectedEntity, cursorWorldPosition, flags);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		// ── 1. Curación completa de salud ──────────────────────────────────────
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(
			entity.FindComponent(SCR_CharacterDamageManagerComponent));

		if (damageManager)
		{
			// Eliminar todos los sangrados activos
			damageManager.RemoveAllBleedings();

			// Restaurar todas las HitZones a salud máxima
			array<HitZone> allHitZones = {};
			damageManager.GetAllHitZones(allHitZones);

			foreach (HitZone hz : allHitZones)
			{
				if (hz && !hz.IsProxy())
					hz.SetHealth(hz.GetMaxHealth());
			}
		}

		// ── 2. Restaurar hambre y sed al máximo ────────────────────────────────
		EL_SurvivalStatsComponent survComp = EL_SurvivalStatsComponent.Cast(
			entity.FindComponent(EL_SurvivalStatsComponent));

		if (survComp)
		{
			survComp.SetFood(100.0);
			survComp.SetWater(100.0);
			survComp.SetStamina(100.0);
		}

		// ── 3. Notificar al jugador ────────────────────────────────────────────
		PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (controller)
		{
			SCR_HintManagerComponent.ShowCustomHint(
				WidgetManager.Translate("#DH-Admin_Heal_Message"),
				WidgetManager.Translate("#DH-Admin_Heal_Title"),
				5.0
			);
		}

		Print(string.Format("[EL_AdminJobContextActions] ADMIN curó completamente a: %1", playerName), LogLevel.NORMAL);
	}
}

//------------------------------------------------------------------------------------------------
//! ADMIN: Revivir a un jugador incapacitado
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminRevivePlayerContextAction : SCR_SelectedEntitiesContextAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		if (selectedEntity.GetEntityType() != EEditableEntityType.CHARACTER)
			return false;

		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return false;

		EL_IncapacitationComponent incap = EL_IncapacitationComponent.Cast(entity.FindComponent(EL_IncapacitationComponent));
		return incap && incap.IsIncapacitated();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition, int flags)
	{
		return CanBeShown(selectedEntity, cursorWorldPosition, flags);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent selectedEntity, vector cursorWorldPosition)
	{
		IEntity entity = selectedEntity.GetOwner();
		if (!entity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		// Reanimar usando el ciclo de respawn completo
		// Esto evita el bug de pantalla negra / pérdida de control del revive in-place
		EL_DeathManagerComponent deathManager = EL_DeathManagerComponent.Cast(entity.FindComponent(EL_DeathManagerComponent));
		if (deathManager)
			deathManager.ReviveViaRespawn();

		Print(string.Format("[EL_AdminJobContextActions] ADMIN revivió a: %1 (via respawn cycle)", playerName), LogLevel.NORMAL);
	}
}


//------------------------------------------------------------------------------------------------
// ================== FORZAR EVENTO AMENAZA ARMADA ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Forzar el inicio del evento Amenaza Armada inmediatamente (admin, sin verificar policías online).
//! Se muestra al hacer click en el suelo cuando el evento NO está activo.
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminForceThreatEventContextAction : SCR_GeneralContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		if (hoveredEntity || !selectedEntities.IsEmpty())
			return false;

		EL_ThreatNPCEventManager mgr = EL_ThreatNPCEventManager.GetInstance();
		if (!mgr)
			return false;

		return !mgr.IsEventActive();
	}

	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		EL_ThreatNPCEventManager mgr = EL_ThreatNPCEventManager.GetInstance();
		return mgr != null && !mgr.IsEventActive();
	}

	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		EL_ThreatNPCEventManager mgr = EL_ThreatNPCEventManager.GetInstance();
		if (!mgr)
		{
			Print("[EL_AdminJobContextActions] ERROR: EL_ThreatNPCEventManager no encontrado en el mundo!", LogLevel.ERROR);
			return;
		}

		Print("[EL_AdminJobContextActions] ADMIN forzó evento AMENAZA ARMADA.", LogLevel.WARNING);
		mgr.ForceEventStart();
	}
}


//------------------------------------------------------------------------------------------------
// ================== FORZAR EVENTO EMERGENCIA EMS ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Forzar el inicio del evento Emergencia EMS inmediatamente (admin, sin verificar médicos online).
//! Se muestra al hacer click en el suelo cuando el evento NO está activo.
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminForceEMSEventContextAction : SCR_GeneralContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		if (hoveredEntity || !selectedEntities.IsEmpty())
			return false;

		EL_EmergencySOSEventManager mgr = EL_EmergencySOSEventManager.GetInstance();
		if (!mgr)
			return false;

		return !mgr.IsEventActive();
	}

	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		EL_EmergencySOSEventManager mgr = EL_EmergencySOSEventManager.GetInstance();
		return mgr != null && !mgr.IsEventActive();
	}

	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		EL_EmergencySOSEventManager mgr = EL_EmergencySOSEventManager.GetInstance();
		if (!mgr)
		{
			Print("[EL_AdminJobContextActions] ERROR: EL_EmergencySOSEventManager no encontrado en el mundo!", LogLevel.ERROR);
			return;
		}

		Print("[EL_AdminJobContextActions] ADMIN forzó evento EMERGENCIA EMS.", LogLevel.WARNING);
		mgr.ForceEventStart();
	}
}


//------------------------------------------------------------------------------------------------
// ================== LISTA DE BANS ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Abre la lista de bans del servidor. Siempre visible para el GM, ejecuta en cliente.
//! Se muestra al hacer click en el suelo (sin entidad seleccionada).
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminOpenBanListContextAction : SCR_GeneralContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		if (hoveredEntity || !selectedEntities.IsEmpty())
			return false;
		return true;
	}

	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		return true;
	}

	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
			menuManager.OpenMenu(ChimeraMenuPreset.EL_BanListMenu);
	}
}


//------------------------------------------------------------------------------------------------
// ================== WHITELIST MAFIA ==================
//------------------------------------------------------------------------------------------------

//! Abre el menú de gestión de whitelist de mafia. Siempre visible para el GM, ejecuta en cliente.
//! Se muestra al hacer click en el suelo (sin entidad seleccionada).
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminOpenMafiaWhitelistContextAction : SCR_GeneralContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		if (hoveredEntity || !selectedEntities.IsEmpty())
			return false;
		return true;
	}

	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		return true;
	}

	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
			menuManager.OpenMenu(ChimeraMenuPreset.EL_MafiaWhitelistMenu);
	}
}


//------------------------------------------------------------------------------------------------
// ================== FORZAR EVENTO BLINDADO ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Forzar el inicio del evento Camión Blindado inmediatamente.
//! Se muestra al hacer click en el suelo cuando el evento NO está activo.
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminForceArmoredTruckEventContextAction : SCR_GeneralContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		if (hoveredEntity || !selectedEntities.IsEmpty())
			return false;

		EL_ArmoredTruckEventManager mgr = EL_ArmoredTruckEventManager.GetInstance();
		if (!mgr)
			return false;

		return !mgr.IsEventActive();
	}

	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		EL_ArmoredTruckEventManager mgr = EL_ArmoredTruckEventManager.GetInstance();
		return mgr != null && !mgr.IsEventActive();
	}

	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		EL_ArmoredTruckEventManager mgr = EL_ArmoredTruckEventManager.GetInstance();
		if (!mgr)
		{
			Print("[EL_AdminJobContextActions] ERROR: EL_ArmoredTruckEventManager no encontrado en el mundo!", LogLevel.ERROR);
			return;
		}

		Print("[EL_AdminJobContextActions] ADMIN forzó evento CAMIÓN BLINDADO.", LogLevel.WARNING);
		mgr.ForceEventStart();
	}
}


//------------------------------------------------------------------------------------------------
// ================== FORZAR EVENTO VUELCO AL NARCO ==================
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Forzar el inicio del evento Vuelco al Narco inmediatamente.
//! Se muestra al hacer click en el suelo cuando el evento NO está activo.
[BaseContainerProps(), SCR_BaseContainerCustomTitleUIInfo("m_Info")]
class EL_AdminForceNarcoAssaultEventContextAction : SCR_GeneralContextAction
{
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		if (hoveredEntity || !selectedEntities.IsEmpty())
			return false;

		EL_NarcoAssaultEventManager mgr = EL_NarcoAssaultEventManager.GetInstance();
		if (!mgr)
			return false;

		return !mgr.IsEventActive();
	}

	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		EL_NarcoAssaultEventManager mgr = EL_NarcoAssaultEventManager.GetInstance();
		return mgr != null && !mgr.IsEventActive();
	}

	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		EL_NarcoAssaultEventManager mgr = EL_NarcoAssaultEventManager.GetInstance();
		if (!mgr)
		{
			Print("[EL_AdminJobContextActions] ERROR: EL_NarcoAssaultEventManager no encontrado en el mundo!", LogLevel.ERROR);
			return;
		}

		Print("[EL_AdminJobContextActions] ADMIN forzó evento VUELCO AL NARCO.", LogLevel.WARNING);
		mgr.ForceEventStart();
	}
}
