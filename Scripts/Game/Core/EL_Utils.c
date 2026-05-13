class EL_Utils : EPF_Utils
{
	//------------------------------------------------------------------------------------------------
	static int MaxInt(int a, int b)
	{
		if (a > b) return a;
		return b;
	}

	//------------------------------------------------------------------------------------------------
	static int MinInt(int a, int b)
	{
		if (a < b) return a;
		return b;
	}

	protected static ref array<IEntity> s_aNearbyEntities = {};
	protected static string s_sRequiredComponent = "";

	//------------------------------------------------------------------------------------------------
	static array<IEntity> GetNearbyVehicles(vector center, float radius, string requiredComponent = "")
	{
		s_aNearbyEntities = new array<IEntity>();
		s_sRequiredComponent = requiredComponent;
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, InsertNearbyEntity, FilterVehicleEntity);
		return s_aNearbyEntities;
	}

	//------------------------------------------------------------------------------------------------
	static array<IEntity> GetNearbyCharacters(vector center, float radius)
	{
		s_aNearbyEntities = new array<IEntity>();
		s_sRequiredComponent = "";
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, InsertNearbyEntity, FilterCharacterEntity);
		return s_aNearbyEntities;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool InsertNearbyEntity(IEntity entity)
	{
		if (!entity)
			return true;

		s_aNearbyEntities.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FilterVehicleEntity(IEntity entity)
	{
		if (!entity || !entity.IsInherited(Vehicle))
			return false;

		if (s_sRequiredComponent.IsEmpty())
			return true;

		typename componentType = s_sRequiredComponent.ToType();
		if (!componentType)
			return true;

		return entity.FindComponent(componentType) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FilterCharacterEntity(IEntity entity)
	{
		return !!SCR_ChimeraCharacter.Cast(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Muestra una notificación al jugador local usando el sistema custom
	//! @param message - Mensaje a mostrar
	//! @param title - Título de la notificación (opcional)
	//! @param duration - Duración en segundos (por defecto 3.0)
	//! @param type - Tipo de notificación (INFO, SUCCESS, ERROR, WARNING, etc.)
	static void Notify(string message, string title = "", float duration = 3.0, EL_ENotificationType type = EL_ENotificationType.INFO)
	{
		// Intentar usar el sistema de notificaciones custom primero
		EL_NotificationManagerComponent notifManager = EL_NotificationManagerComponent.GetInstance();
		if (notifManager)
		{
			// Crear configuración de notificación
			EL_NotificationConfig config = new EL_NotificationConfig(title, message, duration, type);
			
			// Mostrar directamente en cliente local (no enviar por red)
			notifManager.ShowLocalNotification(config);
			return;
		}
		
		// Fallback al sistema de hints si el manager no está disponible
		SCR_HintManagerComponent.ShowCustomHint(message, title, duration);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Versión simplificada para éxito
	static void NotifySuccess(string message, string title = "ÉXITO", float duration = 3.0)
	{
		Notify(message, title, duration, EL_ENotificationType.SUCCESS);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Versión simplificada para error
	static void NotifyError(string message, string title = "ERROR", float duration = 3.0)
	{
		Notify(message, title, duration, EL_ENotificationType.ERROR);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Versión simplificada para advertencia
	static void NotifyWarning(string message, string title = "ADVERTENCIA", float duration = 3.0)
	{
		Notify(message, title, duration, EL_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Debug option: ignorar requisito de policías en misiones/robos
	protected static bool s_bIgnorePoliceRequirement = false;

	static bool IsPoliceRequirementIgnored()
	{
		return s_bIgnorePoliceRequirement;
	}

	static void SetIgnorePoliceRequirement(bool ignore)
	{
		s_bIgnorePoliceRequirement = ignore;
	}

	//------------------------------------------------------------------------------------------------
	//! Obtener el número de policías en servicio (cualquier Job Policía)
	static int GetOnlinePoliceCount()
	{
		int count = 0;
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!playerEntity)
				continue;

			EL_PlayerJobComponent jobComp = EL_PlayerJobComponent.Cast(playerEntity.FindComponent(EL_PlayerJobComponent));
			if (jobComp && jobComp.GetJob() == EL_EJobType.POLICE && EL_PolicePDAManagerComponent.IsOfficerOnDuty(playerId))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	static SCR_ChimeraCharacter GetLocalCharacter()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return SCR_ChimeraCharacter.Cast(controller.GetControlledEntity());
	}

//------------------------------------------------------------------------------------------------
//! Obtiene el ID único del Character (NO el Steam UID)
//! El Character ID es el PersistentId del componente EPF, diferente del Steam UID que identifica la cuenta
static string GetCharacterId(SCR_ChimeraCharacter character)
{
	if (!character)
		return "";

	// Primero intentar obtener el ID del componente de persistencia (fuente de verdad)
	EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(character);
	if (persistence)
	{
		string persistentId = persistence.GetPersistentId();
		if (!persistentId.IsEmpty())
			return persistentId;
	}
	
	// Fallback: buscar en la cuenta del jugador
	PlayerManager playerManager = GetGame().GetPlayerManager();
	if (!playerManager)
		return "";

	int playerId = playerManager.GetPlayerIdFromControlledEntity(character);
	if (playerId <= 0)
		return "";
	
	// Obtener Steam UID para buscar la cuenta
	string steamUID = GetPlayerSteamUID(playerId);
	if (steamUID.IsEmpty())
		return "";
	
	// Obtener la cuenta del jugador
	EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
	if (!accountManager)
		return "";
	
	EL_PlayerAccount account = accountManager.GetFromCache(steamUID);
	if (!account)
		return "";
	
	// Obtener el personaje activo de la cuenta
	EL_PlayerCharacter playerCharacter = account.GetActiveCharacter();
	if (!playerCharacter)
		return "";
	
	// Devolver el ID del Character (GUID), no el Steam UID
	return playerCharacter.GetId();
}

//------------------------------------------------------------------------------------------------
static string GetCharacterId(IEntity entity)
{
	return GetCharacterId(SCR_ChimeraCharacter.Cast(entity));
}

//------------------------------------------------------------------------------------------------
//! Obtiene el ID único del Character por playerId
static string GetCharacterIdFromPlayerId(int playerId)
{
	if (playerId <= 0)
		return "";
	
	// Obtener Steam UID para buscar la cuenta
	string steamUID = GetPlayerSteamUID(playerId);
	if (steamUID.IsEmpty())
		return "";
	
	// Obtener la cuenta del jugador
	EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
	if (!accountManager)
		return "";
	
	EL_PlayerAccount account = accountManager.GetFromCache(steamUID);
	if (!account)
		return "";
	
	// Obtener el personaje activo de la cuenta
	EL_PlayerCharacter playerCharacter = account.GetActiveCharacter();
	if (!playerCharacter)
		return "";
	
	// Devolver el ID del Character (GUID), no el Steam UID
	return playerCharacter.GetId();
}

//------------------------------------------------------------------------------------------------
static string GetCharacterName(SCR_ChimeraCharacter character)
{
	if (!character)
		return "";

	CharacterIdentityComponent identityComp = CharacterIdentityComponent.Cast(character.FindComponent(CharacterIdentityComponent));
	if (identityComp)
	{
		Identity identity = identityComp.GetIdentity();
		if (identity && identity.GetFullName())
			return identity.GetFullName();
	}

	PlayerManager playerManager = GetGame().GetPlayerManager();
	if (playerManager)
	{
		int playerId = playerManager.GetPlayerIdFromControlledEntity(character);
		if (playerId > 0)
		{
			string playerName = playerManager.GetPlayerName(playerId);
			if (playerName && !playerName.IsEmpty())
				return playerName;
		}
	}

	return "";
}

//------------------------------------------------------------------------------------------------
static string GetCharacterName(IEntity entity)
{
	return GetCharacterName(SCR_ChimeraCharacter.Cast(entity));
}

//------------------------------------------------------------------------------------------------
static bool IsCharacterLocal(SCR_ChimeraCharacter character)
{
	if (!character)
		return false;

	RplComponent rplComponent = RplComponent.Cast(character.FindComponent(RplComponent));
	if (!rplComponent)
		return false;

	return rplComponent.IsOwner();
}

//------------------------------------------------------------------------------------------------
static bool IsCharacterLocal(IEntity entity)
{
	return IsCharacterLocal(SCR_ChimeraCharacter.Cast(entity));
}

	//------------------------------------------------------------------------------------------------
	static SCR_ChimeraCharacter FindCharacterById(string characterId)
	{
		if (!characterId || characterId.IsEmpty())
			return null;

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (persistenceManager)
		{
			IEntity resultEntity = persistenceManager.FindEntityByPersistentId(characterId);
			if (resultEntity)
				return SCR_ChimeraCharacter.Cast(resultEntity);
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return null;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity entity = playerManager.GetPlayerControlledEntity(playerId);
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (character && GetCharacterId(character) == characterId)
				return character;
		}

		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Recursively change color on entity and all children using material parameters
	static void ChangeColorRecursive(IEntity entity, Color color)
	{
		if (!entity)
			return;
		
		// Try to set color via signals manager (for vehicles with paintable surfaces)
		SignalsManagerComponent signalsManager = SignalsManagerComponent.Cast(entity.FindComponent(SignalsManagerComponent));
		if (signalsManager)
		{
			// Common vehicle color signals
			int signalR = signalsManager.AddOrFindSignal("VehicleColorR");
			int signalG = signalsManager.AddOrFindSignal("VehicleColorG");
			int signalB = signalsManager.AddOrFindSignal("VehicleColorB");
			
			if (signalR >= 0)
				signalsManager.SetSignalValue(signalR, color.R() / 255.0);
			if (signalG >= 0)
				signalsManager.SetSignalValue(signalG, color.G() / 255.0);
			if (signalB >= 0)
				signalsManager.SetSignalValue(signalB, color.B() / 255.0);
		}
		
		// Apply to children recursively
		IEntity child = entity.GetChildren();
		while (child)
		{
			ChangeColorRecursive(child, color);
			child = child.GetSibling();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set color on vehicle slots
	static void SetSlotsColor(IEntity vehicle, int colorInt)
	{
		if (!vehicle)
			return;
		
		Color color = Color.FromInt(colorInt);
		
		// Find slot manager
		SlotManagerComponent slotManager = SlotManagerComponent.Cast(vehicle.FindComponent(SlotManagerComponent));
		if (!slotManager)
			return;
		
		// Apply color to all slotted entities
		array<EntitySlotInfo> slots = {};
		slotManager.GetSlotInfos(slots);
		
		foreach (EntitySlotInfo slotInfo : slots)
		{
			IEntity slottedEntity = slotInfo.GetAttachedEntity();
			if (slottedEntity)
			{
				ChangeColorRecursive(slottedEntity, color);
			}
		}
	}
	//------------------------------------------------------------------------------------------------
	//! Get player UID - returns the PERSISTENT CHARACTER ID, not the Steam UID
	//! Este método es un alias de GetCharacterId para compatibilidad con código existente
	static override string GetPlayerUID(IEntity player)
	{
		return GetCharacterId(player);
	}

	//------------------------------------------------------------------------------------------------
	//! Get player Steam UID (not character ID) - for account lookups
	static string GetPlayerSteamUID(IEntity player)
	{
		if (!player)
			return "";
			
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";
		
		int playerId = playerManager.GetPlayerIdFromControlledEntity(player);
		if (playerId > 0)
		{
			// Delegar a la versión con playerId que tiene toda la lógica
			return GetPlayerSteamUID(playerId);
		}
		
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Strip lobby/session suffixes from a UID (e.g. _LOBBY appended by Arma Reforger local session)
	//! The Bohemia backend appends _LOBBY when connecting via lobby system during local testing.
	//! The EDF proxy only accepts clean UUIDs → strip suffix to avoid 400 errors.
	static string CleanUID(string uid)
	{
		if (uid.EndsWith("_LOBBY"))
			uid = uid.Substring(0, uid.Length() - 6);
		else if (uid.EndsWith("_GAME"))
			uid = uid.Substring(0, uid.Length() - 5);

		// Normalize to lowercase — the WebProxy MongoDB driver uses Guid.ToString()
		// which outputs lowercase, causing _id mismatch with mixed-case UIDs (500 error)
		uid.ToLower();
		return uid;
	}

	//! Get player Steam UID from player ID - for account lookups
	static string GetPlayerSteamUID(int playerId)
	{
		if (playerId <= 0)
			return "";
			
		string uid = CleanUID(EPF_Utils.GetPlayerUID(playerId));
		if (!uid.IsEmpty())
			return uid;
		
		// Fallback para Workbench/modo local
		// En Workbench, BackendApi.GetPlayerIdentityId() siempre devuelve vacío
		// Usamos un formato de GUID consistente para desarrollo local
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			string playerName = playerManager.GetPlayerName(playerId);
			if (!playerName.IsEmpty())
			{
				// Generar un GUID-like consistente basado en el nombre del jugador
				int hash = Math.AbsInt(playerName.Hash());
				string hashStr = hash.ToString();
				
				// Pad manual a 12 dígitos
				while (hashStr.Length() < 12)
				{
					hashStr = "0" + hashStr;
				}
				if (hashStr.Length() > 12)
					hashStr = hashStr.Substring(0, 12);
				
				// Formato GUID: 00000000-0000-0000-0000-xxxxxxxxxxxx
				string localUID = "00000000-0000-0000-0000-" + hashStr;
				Print(string.Format("[EL_Utils] Workbench mode - Using local GUID for player %1: %2", playerName, localUID), LogLevel.WARNING);
				return localUID;
			}
		}
		
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Get player name (wrapper for GetCharacterName)
	static string GetPlayerName(IEntity player)
	{
		return GetCharacterName(player);
	}
};
