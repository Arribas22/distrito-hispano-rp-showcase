//------------------------------------------------------------------------------------------------
//! UI RPC Component
//! Componente para enviar RPCs desde el servidor al cliente para abrir menÃºs
//! Se adjunta al PlayerController para tener capacidades de RPC
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Core", description: "Handles RPC calls for opening client UI")]
class EL_UIRPCComponentClass : ScriptComponentClass
{
}

class EL_CharacterPreviewLoadoutRequestContext
{
	string m_sCharacterId;
	int m_iRequestId;

	void EL_CharacterPreviewLoadoutRequestContext(string characterId, int requestId)
	{
		m_sCharacterId = characterId;
		m_iRequestId = requestId;
	}
}

class EL_CharacterStatsRequestContext
{
	string m_sCharacterId;
	int m_iRequestId;

	void EL_CharacterStatsRequestContext(string characterId, int requestId)
	{
		m_sCharacterId = characterId;
		m_iRequestId = requestId;
	}
}

class EL_UIRPCComponent : ScriptComponent
{
	// Fallback client-side: detectar si el RPC inicial nunca llego
	protected bool m_bInitialMenuReceived = false;
	protected static const int INITIAL_MENU_FALLBACK_DELAY_MS = 4000;

	//------------------------------------------------------------------------------------------------
	//! SERVER: Send a hint to the owner client via Owner RPC
	void SendHint(string message, string title, float duration)
	{
		if (!Replication.IsServer())
			return;
		
		Rpc(RPC_SendHint_Owner, message, title, duration);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_SendHint_Owner(string message, string title, float duration)
	{
		SCR_HintManagerComponent.ShowCustomHint(message, title, duration);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al cliente para abrir WelcomeMenu
	//! Se llama desde el SERVIDOR
	void RPC_OpenWelcomeMenu(int playerId, string playerUID)
	{
		//Print(string.Format("[EL_UIRPCComponent] Sending RPC to open WelcomeMenu for player %1", playerId), LogLevel.NORMAL);
		Rpc(RPC_OpenWelcomeMenu_Owner, playerId, playerUID);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el CLIENTE para abrir WelcomeMenu
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenWelcomeMenu_Owner(int playerId, string playerUID)
	{
		m_bInitialMenuReceived = true;
		TryOpenWelcomeMenu(playerId, playerUID, 0);
	}
	
	//! Intenta abrir el WelcomeMenu con reintentos si el MenuManager aun no esta disponible
	protected void TryOpenWelcomeMenu(int playerId, string playerUID, int retries)
	{
		const int MAX_RETRIES = 3;
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			if (retries < MAX_RETRIES)
				GetGame().GetCallqueue().CallLater(TryOpenWelcomeMenu, 500, false, playerId, playerUID, retries + 1);
			else
				Print("[EL_UIRPCComponent] ERROR: MenuManager no disponible tras " + MAX_RETRIES + " reintentos. El jugador puede quedar atrapado.", LogLevel.ERROR);
			return;
		}
		
		EL_WelcomeMenu menu = EL_WelcomeMenu.Cast(menuManager.OpenMenu(ChimeraMenuPreset.WelcomeMenu));
		if (menu)
		{
			menu.SetPlayerData(playerId, playerUID);
		}
		else if (retries < MAX_RETRIES)
		{
			GetGame().GetCallqueue().CallLater(TryOpenWelcomeMenu, 500, false, playerId, playerUID, retries + 1);
		}
		else
		{
			Print("[EL_UIRPCComponent] ERROR: No se pudo abrir WelcomeMenu tras " + MAX_RETRIES + " reintentos.", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Abrir CharacterSelectionMenu (jugador tiene account pero necesita elegir/crear character)
	//! En servidor dedicado enviamos los datos como arrays paralelos de primitivos (replicables)
	void RPC_OpenCharacterSelectionMenu(int playerId, string playerUID, EL_PlayerAccount account)
	{
		//Print(string.Format("[EL_UIRPCComponent] Sending RPC to open CharacterSelectionMenu for player %1", playerId), LogLevel.NORMAL);
		
		// Serializar a arrays paralelos (tipos primitivos son replicables)
		array<string> ids = {};
		array<string> firstNames = {};
		array<string> lastNames = {};
		array<int> ages = {};
		array<string> prefabs = {}; // ResourceName se envÃ­a como string
		array<bool> hasSpawned = {};
		
		array<ref EL_PlayerCharacter> characters = account.GetCharacters();
		foreach (EL_PlayerCharacter character : characters)
		{
			ids.Insert(character.GetId());
			firstNames.Insert(character.GetFirstName());
			lastNames.Insert(character.GetLastName());
			ages.Insert(character.GetAge());
			prefabs.Insert(character.GetPrefab());
			hasSpawned.Insert(character.HasSpawnedBefore());
		}
		
		Rpc(RPC_OpenCharacterSelectionMenu_Owner, playerId, playerUID, ids, firstNames, lastNames, ages, prefabs, hasSpawned);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el CLIENTE para abrir CharacterSelectionMenu
	//! Recibe los datos como arrays paralelos de primitivos
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenCharacterSelectionMenu_Owner(int playerId, string playerUID, array<string> ids, array<string> firstNames, array<string> lastNames, array<int> ages, array<string> prefabs, array<bool> hasSpawned)
	{
		int charCount = ids.Count();
		//Print(string.Format("[EL_UIRPCComponent] RPC received on CLIENT - Opening CharacterSelectionMenu for player %1 with %2 characters", playerId, charCount), LogLevel.NORMAL);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			Print("[EL_UIRPCComponent] ERROR: MenuManager not found on client!", LogLevel.ERROR);
			return;
		}
		
		EL_CharacterSelectionMenu menu = EL_CharacterSelectionMenu.Cast(
			menuManager.OpenMenu(ChimeraMenuPreset.CharacterSelectionMenu)
		);
		
		if (menu)
		{
			// Pasar los datos primitivos directamente al menÃº, sin crear objetos EPF
			menu.SetPlayerData(playerId, playerUID, ids, firstNames, lastNames, ages, prefabs, hasSpawned);
			//Print(string.Format("[EL_UIRPCComponent] âœ“ CharacterSelectionMenu opened for player %1 with %2 characters", playerId, charCount), LogLevel.NORMAL);
		}
		else
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open CharacterSelectionMenu!", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al cliente para abrir DeathMenu
	void RPC_OpenDeathMenu(int playerId, float timeRemaining)
	{
		//Print(string.Format("[EL_UIRPCComponent] Sending RPC to open DeathMenu for player %1 with %2 seconds", playerId, timeRemaining), LogLevel.NORMAL);
		Rpc(RPC_OpenDeathMenu_Owner, playerId, timeRemaining);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el CLIENTE para abrir DeathMenu
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenDeathMenu_Owner(int playerId, float timeRemaining)
	{
		//Print(string.Format("[EL_UIRPCComponent] RPC received on CLIENT - Opening DeathMenu for player %1", playerId), LogLevel.NORMAL);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			Print("[EL_UIRPCComponent] ERROR: MenuManager not found on client!", LogLevel.ERROR);
			return;
		}
		
		MenuBase menu = menuManager.OpenMenu(ChimeraMenuPreset.DeathMenu);
		EL_DeathMenuUI deathMenu = EL_DeathMenuUI.Cast(menu);
		if (deathMenu)
		{
			//Print(string.Format("[EL_UIRPCComponent] âœ“ DeathMenu opened on client for player %1 with %2 seconds", playerId, timeRemaining), LogLevel.NORMAL);
		}
		else
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open DeathMenu on client!", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al servidor para procesar el boton Continuar del WelcomeMenu
	//! Se llama desde el CLIENTE
	void RPC_ProcessWelcomeMenuContinue()
	{
		//Print("[EL_UIRPCComponent] Sending RPC to server to process WelcomeMenu continue", LogLevel.NORMAL);
		Rpc(RPC_ProcessWelcomeMenuContinue_Server);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el SERVIDOR para procesar el evento Continuar
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_ProcessWelcomeMenuContinue_Server()
	{
		//Print("[EL_UIRPCComponent] RPC received on SERVER - Processing WelcomeMenu continue", LogLevel.NORMAL);
		
		// Obtener el PlayerController owner de este componente
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: Owner is not a PlayerController!", LogLevel.ERROR);
			return;
		}
		
		int playerId = playerController.GetPlayerId();
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);
		
		// âœ… El account SIEMPRE debe existir en este punto (se crea en OnAccountLoadedForSpawn)
		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerUID);
		
		if (account)
		{
			// Cuenta ya en cache
			//Print(string.Format("[EL_UIRPCComponent] Account found in cache for player %1, hasCharacters=%2", playerId, account.HasCharacters()), LogLevel.NORMAL);
			ProcessAccountLoaded(playerId, playerUID, account);
		}
		else
		{
			// âš ï¸ ADVERTENCIA: El account deberÃ­a existir - fue creado al conectarse
			// Si no existe, intentar cargarlo de la DB como fallback
			Print(string.Format("[EL_UIRPCComponent] WARNING: Account not in cache for player %1 - loading from DB", playerId), LogLevel.WARNING);
			EDF_DataCallbackSingle<EL_PlayerAccount> callback(this, "OnAccountLoadedForWelcomeMenu");
			EL_PlayerAccountManager.GetInstance().LoadAccountAsync(playerUID, false, callback);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback cuando se carga la cuenta desde el servidor
	//! IMPORTANTE: Debe ser pÃºblico para que EDF_DataCallbackSingle pueda llamarlo
	void OnAccountLoadedForWelcomeMenu(EL_PlayerAccount account, Managed context)
	{
		if (!account)
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to load/create account", LogLevel.ERROR);
			return;
		}
		
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: Owner is not a PlayerController!", LogLevel.ERROR);
			return;
		}
		
		int playerId = playerController.GetPlayerId();
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);
		
		ProcessAccountLoaded(playerId, playerUID, account);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Procesar la cuenta cargada y abrir el menu correspondiente
	protected void ProcessAccountLoaded(int playerId, string playerUID, EL_PlayerAccount account)
	{
		//Print(string.Format("[EL_UIRPCComponent] Processing account for player %1, hasCharacters=%2", playerId, account.HasCharacters()), LogLevel.NORMAL);
		
		if (account.HasCharacters())
		{
			// Abrir menu de seleccion de personaje (enviando datos del account)
			RPC_OpenCharacterSelectionMenu(playerId, playerUID, account);
		}
		else
		{
			// Abrir menu de creacion de personaje
			RPC_OpenCharacterCreationMenu(playerId, playerUID);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al cliente para abrir CharacterCreationMenu
	void RPC_OpenCharacterCreationMenu(int playerId, string playerUID)
	{
		//Print(string.Format("[EL_UIRPCComponent] Sending RPC to open CharacterCreationMenu for player %1", playerId), LogLevel.NORMAL);
		Rpc(RPC_OpenCharacterCreationMenu_Owner, playerId, playerUID);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el CLIENTE para abrir CharacterCreationMenu
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenCharacterCreationMenu_Owner(int playerId, string playerUID)
	{
		//Print(string.Format("[EL_UIRPCComponent] RPC received on CLIENT - Opening CharacterCreationMenu for player %1", playerId), LogLevel.NORMAL);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			Print("[EL_UIRPCComponent] ERROR: MenuManager not found on client!", LogLevel.ERROR);
			return;
		}
		
		// Abrir el menu de creacion de personaje (solo UI, sin account)
		EL_CharacterCreationMenu menu = EL_CharacterCreationMenu.Cast(
			menuManager.OpenMenu(ChimeraMenuPreset.CharacterCreationMenu)
		);
		
		if (menu)
		{
			menu.SetPlayerData(playerId, playerUID);
			//Print(string.Format("[EL_UIRPCComponent] CharacterCreationMenu opened on client for player %1", playerId), LogLevel.NORMAL);
		}
		else
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open CharacterCreationMenu on client!", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al servidor para crear un nuevo personaje
	//! Se llama desde el CLIENTE
	void RPC_CreateCharacter(string firstName, string lastName, int age, int race)
	{
		//Print(string.Format("[EL_UIRPCComponent] Sending RPC to server to create character: %1 %2, age %3, race %4", firstName, lastName, age, race), LogLevel.NORMAL);
		Rpc(RPC_CreateCharacter_Server, firstName, lastName, age, race);
	}

	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al servidor para crear un nuevo personaje usando prefab de apariencia
	//! Se llama desde el CLIENTE
	void RPC_CreateCharacterWithPrefab(string firstName, string lastName, int age, ResourceName appearancePrefab, int legacyRace, ResourceName beardPrefab = "")
	{
		Rpc(RPC_CreateCharacterWithPrefab_Server, firstName, lastName, age, appearancePrefab, legacyRace, beardPrefab);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el SERVIDOR para crear personaje
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_CreateCharacter_Server(string firstName, string lastName, int age, int race)
	{
		//Print(string.Format("[EL_UIRPCComponent] RPC received on SERVER - Creating character: %1 %2", firstName, lastName), LogLevel.NORMAL);
		
		// Obtener el PlayerController owner de este componente
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: Owner is not a PlayerController!", LogLevel.ERROR);
			return;
		}
		
		int playerId = playerController.GetPlayerId();
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);
		
		// Obtener/cargar la cuenta del jugador
		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerUID);
		if (!account)
		{
			Print(string.Format("[EL_UIRPCComponent] ERROR: Account not found in cache for player %1", playerId), LogLevel.ERROR);
			return;
		}
		
		//Print(string.Format("[EL_UIRPCComponent] Account found: %1, current character count: %2", playerUID, account.GetCharacterCount()), LogLevel.NORMAL);
		
		// Obtener el prefab basado en la apariencia seleccionada
		ResourceName prefab = GetPrefabForRace(race);
		//Print(string.Format("[EL_UIRPCComponent] Selected prefab for race %1: %2", race, prefab), LogLevel.NORMAL);
		
		// Crear el personaje
		EL_PlayerCharacter character = EL_PlayerCharacter.Create(prefab);
		if (!character)
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to create character!", LogLevel.ERROR);
			return;
		}
		
		character.SetFirstName(firstName);
		character.SetLastName(lastName);
		character.SetAge(age);
		
		//Print(string.Format("[EL_UIRPCComponent] Character created with ID: %1, Name: %2", character.GetId(), character.GetFullName()), LogLevel.NORMAL);
		
		// Agregar a la cuenta y guardar
		account.AddCharacter(character, true);
		//Print(string.Format("[EL_UIRPCComponent] Character added to account, new count: %1, active index: %2", account.GetCharacterCount(), account.GetActiveCharacterIndex()), LogLevel.NORMAL);
		
		// Guardar la cuenta (que incluye el character en el array)
		bool accountSaved = account.SafeSave();
		//Print(string.Format("[EL_UIRPCComponent] Account.SafeSave() result: %1", accountSaved), LogLevel.NORMAL);
		
		// NOTE: Do NOT cleanup the lobby character here. Deleting the lobby entity before
		// the player has a new controlled entity causes EPF to trigger DoSpawn_S again,
		// which reopens the welcome/creation menus (double-spawn bug).
		// The cleanup already runs in RPC_SpawnCharacterAtPosition_Server immediately
		// before spawning + handing over the real character - that is the correct moment.

		// âœ… ABRIR SPAWN SELECTION MENU para que el jugador seleccione dÃ³nde aparecer
		// NO spawneamos directamente - dejamos que el jugador elija su punto de spawn
		// EPF sincronizarÃ¡ automÃ¡ticamente la cuenta al cliente despuÃ©s del SafeSave()
		//Print(string.Format("[EL_UIRPCComponent] Opening SpawnSelectionMenu for player %1", playerId), LogLevel.NORMAL);
		
		// âœ… Enviar datos del character directamente en el RPC (no depender del cache)
		// Extraer datos del character para enviar como primitivos
		string charId = character.GetId();
		string charFirstName = character.GetFirstName();
		string charLastName = character.GetLastName();
		int charAge = character.GetAge();
		string charPrefab = character.GetPrefab(); // ResourceName convertido a string
		
		RPC_OpenSpawnSelectionMenu(playerId, playerUID, charId, charFirstName, charLastName, charAge, charPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el SERVIDOR para crear personaje con prefab explícito
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_CreateCharacterWithPrefab_Server(string firstName, string lastName, int age, ResourceName appearancePrefab, int legacyRace, ResourceName beardPrefab)
	{
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: Owner is not a PlayerController!", LogLevel.ERROR);
			return;
		}

		int playerId = playerController.GetPlayerId();
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);

		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerUID);
		if (!account)
		{
			Print(string.Format("[EL_UIRPCComponent] ERROR: Account not found in cache for player %1", playerId), LogLevel.ERROR);
			return;
		}

		ResourceName prefab = appearancePrefab;
		if (prefab.IsEmpty() || !IsAllowedCharacterPrefab(prefab))
			prefab = GetPrefabForRace(legacyRace);

		// Validar barba
		ResourceName validatedBeard = "";
		if (!beardPrefab.IsEmpty() && IsAllowedBeardPrefab(beardPrefab))
			validatedBeard = beardPrefab;

		EL_PlayerCharacter character = EL_PlayerCharacter.Create(prefab);
		if (!character)
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to create character!", LogLevel.ERROR);
			return;
		}

		character.SetFirstName(firstName);
		character.SetLastName(lastName);
		character.SetAge(age);
		character.SetBeardPrefab(validatedBeard);

		account.AddCharacter(character, true);
		account.SafeSave();

		string charId = character.GetId();
		string charFirstName = character.GetFirstName();
		string charLastName = character.GetLastName();
		int charAge = character.GetAge();
		string charPrefab = character.GetPrefab();

		RPC_OpenSpawnSelectionMenu(playerId, playerUID, charId, charFirstName, charLastName, charAge, charPrefab);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al cliente para abrir SpawnSelectionMenu con datos primitivos
	void RPC_OpenSpawnSelectionMenu(int playerId, string playerUID, string characterId, string firstName, string lastName, int age, string prefab)
	{
		//Print(string.Format("[EL_UIRPCComponent] Sending RPC to open SpawnSelectionMenu for player %1, character=%2 %3", playerId, firstName, lastName), LogLevel.NORMAL);
		Rpc(RPC_OpenSpawnSelectionMenu_Owner, playerId, playerUID, characterId, firstName, lastName, age, prefab);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el CLIENTE para abrir SpawnSelectionMenu
	//! Recibe datos primitivos, NO objetos EPF
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenSpawnSelectionMenu_Owner(int playerId, string playerUID, string characterId, string firstName, string lastName, int age, string prefab)
	{
		//Print(string.Format("[EL_UIRPCComponent] RPC received on CLIENT - Opening SpawnSelectionMenu for player %1, character=%2 %3", playerId, firstName, lastName), LogLevel.NORMAL);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			Print("[EL_UIRPCComponent] ERROR: MenuManager not found on client!", LogLevel.ERROR);
			return;
		}
		
		EL_SpawnSelectionMenu menu = EL_SpawnSelectionMenu.Cast(
			menuManager.OpenMenu(ChimeraMenuPreset.SpawnSelectionMenu)
		);
		
		if (menu)
		{
			// Pasar solo los datos primitivos necesarios
			menu.SetPlayerData(playerId, playerUID, characterId, firstName, lastName, age, prefab);
			//Print(string.Format("[EL_UIRPCComponent] âœ“ SpawnSelectionMenu opened on client for player %1", playerId), LogLevel.NORMAL);
		}
		else
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open SpawnSelectionMenu on client!", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Obtener prefab segun la apariencia (legacy)
	protected ResourceName GetPrefabForRace(int race)
	{
		switch (race)
		{
			case 0: return "{CE23D4366B47E9B9}Prefabs/Characters/Presets/White_Male_01.et"; // Caucasian
			case 1: return "{BFD6AFB8A1D8E2F9}Prefabs/Characters/Presets/Black_Male_02.et"; // African
			case 2: return "{C465F15DDF56A6D8}Prefabs/Characters/Presets/Asian_Male_02.et"; // Asian
		}
		
		// Default: White
		return "{CE23D4366B47E9B9}Prefabs/Characters/Presets/White_Male_01.et";
	}

	//------------------------------------------------------------------------------------------------
	//! Seguridad: lista blanca de prefabs de personaje permitidos para creación.
	protected bool IsAllowedCharacterPrefab(ResourceName prefab)
	{
		// Prefabs originales
		if (prefab == "{CE23D4366B47E9B9}Prefabs/Characters/Presets/White_Male_01.et") return true;
		if (prefab == "{A277E0D6D7836697}Prefabs/Characters/Presets/White_Male_02.et") return true;
		if (prefab == "{BFD6AFB8A1D8E2F9}Prefabs/Characters/Presets/Black_Male_02.et") return true;
		if (prefab == "{C465F15DDF56A6D8}Prefabs/Characters/Presets/Asian_Male_02.et") return true;
		// Nuevos prefabs de aspecto (11 caras)
		if (prefab == "{6890ABCD12345601}Prefabs/Characters/Presets/Male_Face_White_02.et") return true;
		if (prefab == "{6890ABCD12345602}Prefabs/Characters/Presets/Male_Face_White_10.et") return true;
		if (prefab == "{6890ABCD12345603}Prefabs/Characters/Presets/Male_Face_White_09.et") return true;
		if (prefab == "{6890ABCD12345604}Prefabs/Characters/Presets/Male_Face_White_08.et") return true;
		if (prefab == "{6890ABCD12345605}Prefabs/Characters/Presets/Male_Face_White_06.et") return true;
		if (prefab == "{6890ABCD12345606}Prefabs/Characters/Presets/Male_Face_White_05.et") return true;
		if (prefab == "{6890ABCD12345607}Prefabs/Characters/Presets/Male_Face_White_03_Old.et") return true;
		if (prefab == "{6890ABCD12345608}Prefabs/Characters/Presets/Male_Face_White_03.et") return true;

		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Seguridad: lista blanca de prefabs de barba permitidos para creación.
	protected bool IsAllowedBeardPrefab(ResourceName prefab)
	{
		if (prefab == "{DAB5572FF7A71F20}BeardMustache/_Prefabs/Barba_01.et") return true;
		if (prefab == "{B6E163CF4B63900E}BeardMustache/_Prefabs/Barba_02.et") return true;
		if (prefab == "{537D2F3647860765}BeardMustache/_Prefabs/Barba_03.et") return true;
		if (prefab == "{6E490A0E32EA8E52}BeardMustache/_Prefabs/Barba_04.et") return true;
		if (prefab == "{8BD546F73E0F1939}BeardMustache/_Prefabs/Barba_05.et") return true;
		if (prefab == "{5C57D211F701891E}BeardMustache/_Prefabs/Beard 02.et") return true;
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al servidor para spawnear character en posiciÃ³n seleccionada
	void RPC_SpawnCharacterAtPosition(string characterId, ResourceName prefab, vector position, vector rotation)
	{
		//Print(string.Format("[EL_UIRPCComponent] Sending RPC to server to spawn character at %1", position), LogLevel.NORMAL);
		Rpc(RPC_SpawnCharacterAtPosition_Server, characterId, prefab, position, rotation);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el SERVIDOR para spawnear character
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_SpawnCharacterAtPosition_Server(string characterId, ResourceName prefab, vector position, vector rotation)
	{
		//Print(string.Format("[EL_UIRPCComponent] RPC received on SERVER - Spawning character %1 at %2", characterId, position), LogLevel.NORMAL);
		
		// Obtener el PlayerController owner de este componente
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: Owner is not a PlayerController!", LogLevel.ERROR);
			return;
		}
		
		int playerId = playerController.GetPlayerId();
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);
		
		// Obtener el spawn logic
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.Cast(
			GetGame().GetGameMode().FindComponent(SCR_RespawnSystemComponent)
		);
		
		if (!respawnSystem)
		{
			Print("[EL_UIRPCComponent] ERROR: No respawn system found!", LogLevel.ERROR);
			return;
		}
		
		EL_SpawnLogic spawnLogic = EL_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());
		if (!spawnLogic)
		{
			Print("[EL_UIRPCComponent] ERROR: No spawn logic found!", LogLevel.ERROR);
			return;
		}
		
		// âš ï¸ CRITICAL: Get correct prefab from PlayerAccount, NOT from RPC
		// RPC prefab may be incorrect or from lobby character
		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerUID);
		if (!account)
		{
			Print(string.Format("[EL_UIRPCComponent] ERROR: Account not found for player %1", playerUID), LogLevel.ERROR);
			return;
		}
		
		EL_PlayerCharacter playerChar = account.GetCharacterByUID(characterId);
		if (!playerChar)
		{
			Print(string.Format("[EL_UIRPCComponent] ERROR: Character %1 not found!", characterId), LogLevel.ERROR);
			return;
		}
		
		// Get correct prefab from character metadata
		string prefabString = playerChar.GetPrefab();
		ResourceName correctPrefab = prefabString;
		
		//Print(string.Format("[EL_UIRPCComponent] Using prefab from PlayerAccount: %1", prefabString), LogLevel.NORMAL);
		
		// âš ï¸ CRÃTICO FIX: Verificar si el personaje ya ha sido spawneado antes
		// Si HasSpawnedBefore=true â†’ Usar CreatePlayerCharacter (carga datos de DB)
		// Si HasSpawnedBefore=false â†’ Primer spawn, NO usar CreatePlayerCharacter (causarÃ­a duplicaciÃ³n)
		bool hasSpawnedBefore = playerChar.HasSpawnedBefore();
		
		if (hasSpawnedBefore)
		{
			// Personaje ya spawneÃ³ antes - usar CreatePlayerCharacter para cargar datos guardados
			//Print(string.Format("[EL_UIRPCComponent] Character has spawned before - using CreatePlayerCharacter to load saved data", characterId), LogLevel.NORMAL);
			spawnLogic.CreatePlayerCharacter(playerId, characterId, correctPrefab, position, rotation, false, playerChar);
		}
		else
		{
			// âœ… PRIMER SPAWN del personaje - spawnear directamente sin CreatePlayerCharacter
			// CreatePlayerCharacter causarÃ­a duplicaciÃ³n porque buscarÃ­a en DB y encontrarÃ­a el registro
			// que se creÃ³ en RPC_CreateCharacter_Server
			//Print(string.Format("[EL_UIRPCComponent] FIRST SPAWN of character - spawning directly without CreatePlayerCharacter"), LogLevel.NORMAL);
			spawnLogic.SpawnNewCharacterDirectly(playerId, characterId, correctPrefab, position, rotation, playerChar);
		}
		
		// Actualizar metadata en el account (ya tenemos account del cÃ³digo anterior)
		playerChar.SetHasSpawnedBefore(true);
		playerChar.SetLastSpawnPosition(position);
		playerChar.SetLastSpawnRotation(rotation);
		account.SafeSave();
		//Print(string.Format("[EL_UIRPCComponent] âœ“ Character metadata updated for %1", characterId), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC enviado desde el CLIENTE cuando selecciona un character existente
	void RPC_SelectCharacter(string characterId)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT sending RPC to select character: %1", characterId), LogLevel.NORMAL);
		Rpc(RPC_SelectCharacter_Server, characterId);
	}

	//------------------------------------------------------------------------------------------------
	//! RPC enviado desde el CLIENTE para borrar un personaje existente del PlayerAccount
	void RPC_DeleteCharacter(string characterId)
	{
		Rpc(RPC_DeleteCharacter_Server, characterId);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: pedir outfit de preview guardado en DB para un character
	void RPC_RequestCharacterPreviewLoadout(string characterId, int requestId)
	{
		if (characterId.IsEmpty())
			return;

		Print(string.Format("[EL_UIRPCComponent] RPC_RequestCharacterPreviewLoadout requesting charId=%1 requestId=%2", characterId, requestId), LogLevel.NORMAL);
		Rpc(RPC_RequestCharacterPreviewLoadout_Server, characterId, requestId);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: recibe petición de preview outfit y consulta DB
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestCharacterPreviewLoadout_Server(string characterId, int requestId)
	{
		if (characterId.IsEmpty())
			return;

		ref EL_CharacterPreviewLoadoutRequestContext context = new EL_CharacterPreviewLoadoutRequestContext(characterId, requestId);
		EDF_DbFindCallbackSingle<EPF_CharacterSaveData> callback(this, "OnCharacterPreviewLoadoutDataLoaded", context);
		EPF_PersistenceEntityHelper<EPF_CharacterSaveData>.GetRepository().FindAsync(characterId, callback);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: callback tras cargar datos del personaje desde DB
	void OnCharacterPreviewLoadoutDataLoaded(EDF_EDbOperationStatusCode statusCode, EPF_CharacterSaveData characterData, Managed context)
	{
		EL_CharacterPreviewLoadoutRequestContext requestContext = EL_CharacterPreviewLoadoutRequestContext.Cast(context);
		if (!requestContext)
			return;

		Print(string.Format("[EL_UIRPCComponent] OnCharacterPreviewLoadoutDataLoaded status=%1 charId=%2 requestId=%3 hasData=%4", statusCode, requestContext.m_sCharacterId, requestContext.m_iRequestId, characterData != null), LogLevel.NORMAL);

		string jacketPrefab;
		string vestPrefab;
		string pantsPrefab;
		string bootsPrefab;

		if (statusCode == EDF_EDbOperationStatusCode.SUCCESS && characterData)
			CollectPreviewClothingFromSaveData(characterData, jacketPrefab, vestPrefab, pantsPrefab, bootsPrefab);

		Rpc(RPC_ReceiveCharacterPreviewLoadout_Owner, requestContext.m_sCharacterId, requestContext.m_iRequestId, jacketPrefab, vestPrefab, pantsPrefab, bootsPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: recibe outfit del personaje seleccionado y lo pasa al menu si está abierto
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ReceiveCharacterPreviewLoadout_Owner(string characterId, int requestId, string jacketPrefab, string vestPrefab, string pantsPrefab, string bootsPrefab)
	{
		Print(string.Format("[EL_UIRPCComponent] RPC_ReceiveCharacterPreviewLoadout_Owner charId=%1 requestId=%2 j=%3 v=%4 p=%5 b=%6", characterId, requestId, jacketPrefab, vestPrefab, pantsPrefab, bootsPrefab), LogLevel.NORMAL);
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MenuBase currentMenu = menuManager.GetTopMenu();
		if (!currentMenu)
			return;

		EL_CharacterSelectionMenu selectionMenu = EL_CharacterSelectionMenu.Cast(currentMenu);
		if (!selectionMenu)
		{
			Print(string.Format("[EL_UIRPCComponent] WARN: top menu cast to EL_CharacterSelectionMenu FAILED. topMenu type=%1", currentMenu.Type()), LogLevel.WARNING);
			return;
		}

		Print("[EL_UIRPCComponent] Calling OnCharacterPreviewLoadoutReceived on selection menu", LogLevel.NORMAL);
		selectionMenu.OnCharacterPreviewLoadoutReceived(characterId, requestId, jacketPrefab, vestPrefab, pantsPrefab, bootsPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Extrae posibles prendas equipadas (top/pants/boots) desde save-data EPF
	protected void CollectPreviewClothingFromSaveData(EPF_EntitySaveData entityData, out string jacketPrefab, out string vestPrefab, out string pantsPrefab, out string bootsPrefab)
	{
		if (!entityData)
			return;

		string prefabPath = entityData.m_rPrefab;
		EL_UIRPCComponent.TryAssignPreviewClothingPrefab(prefabPath, jacketPrefab, vestPrefab, pantsPrefab, bootsPrefab);

		if (!entityData.m_aComponents)
			return;

		foreach (EPF_PersistentComponentSaveData persistentComp : entityData.m_aComponents)
		{
			if (!persistentComp || !persistentComp.m_pData)
				continue;

			EPF_BaseInventoryStorageComponentSaveData invSaveData = EPF_BaseInventoryStorageComponentSaveData.Cast(persistentComp.m_pData);
			if (!invSaveData || !invSaveData.m_aSlots)
				continue;

			foreach (EPF_PersistentInventoryStorageSlot slotData : invSaveData.m_aSlots)
			{
				if (!slotData || !slotData.m_pEntity)
					continue;

				CollectPreviewClothingFromSaveData(slotData.m_pEntity, jacketPrefab, vestPrefab, pantsPrefab, bootsPrefab);

				if (!jacketPrefab.IsEmpty() && !vestPrefab.IsEmpty() && !pantsPrefab.IsEmpty() && !bootsPrefab.IsEmpty())
					return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Clasifica un prefab por tipo de prenda para previsualización
	static void TryAssignPreviewClothingPrefab(string prefabPath, out string jacketPrefab, out string vestPrefab, out string pantsPrefab, out string bootsPrefab)
	{
		if (prefabPath.IsEmpty())
			return;

		if (vestPrefab.IsEmpty() && (prefabPath.Contains("Vest") || prefabPath.Contains("/Vests/") || prefabPath.Contains("PlateCarrier") || prefabPath.Contains("TacticalVest") || prefabPath.Contains("_Vest")))
		{
			vestPrefab = prefabPath;
			return;
		}

		if (jacketPrefab.IsEmpty() && (prefabPath.Contains("Jacket") || prefabPath.Contains("Shirt") || prefabPath.Contains("/Tops/") || prefabPath.Contains("Mayka") || prefabPath.Contains("Tracksuit") || prefabPath.Contains("TrackSuit") || prefabPath.Contains("Uniform")))
		{
			jacketPrefab = prefabPath;
			return;
		}

		if (pantsPrefab.IsEmpty() && (prefabPath.Contains("Pants") || prefabPath.Contains("Jeans") || prefabPath.Contains("Trousers") || prefabPath.Contains("jogger") || prefabPath.Contains("Jogger") || prefabPath.Contains("slimpants") || prefabPath.Contains("slimfit") || prefabPath.Contains("Slimfit")))
		{
			pantsPrefab = prefabPath;
			return;
		}

		if (bootsPrefab.IsEmpty() && (prefabPath.Contains("Boots") || prefabPath.Contains("Shoes") || prefabPath.Contains("/Footwear/")))
			bootsPrefab = prefabPath;
	}

	//------------------------------------------------------------------------------------------------
	// ===== CHARACTER STATS FOR SELECTION MENU =====
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! CLIENT: pedir stats de un character guardado en DB
	void RPC_RequestCharacterStats(string characterId, int requestId)
	{
		if (characterId.IsEmpty())
			return;

		Rpc(RPC_RequestCharacterStats_Server, characterId, requestId);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: recibe petición de stats y consulta DB
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestCharacterStats_Server(string characterId, int requestId)
	{
		if (characterId.IsEmpty())
			return;

		ref EL_CharacterStatsRequestContext context = new EL_CharacterStatsRequestContext(characterId, requestId);
		EDF_DbFindCallbackSingle<EPF_CharacterSaveData> callback(this, "OnCharacterStatsDataLoaded", context);
		EPF_PersistenceEntityHelper<EPF_CharacterSaveData>.GetRepository().FindAsync(characterId, callback);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: callback tras cargar datos del personaje para extraer stats
	void OnCharacterStatsDataLoaded(EDF_EDbOperationStatusCode statusCode, EPF_CharacterSaveData characterData, Managed context)
	{
		EL_CharacterStatsRequestContext requestContext = EL_CharacterStatsRequestContext.Cast(context);
		if (!requestContext)
			return;

		int level = 0;
		int money = 0;
		string jobName = WidgetManager.Translate("#DH-Job_Unemployed");
		string groupName = "";
		int vehicleCount = 0;
		bool hasApartment = false;

		if (statusCode == EDF_EDbOperationStatusCode.SUCCESS && characterData && characterData.m_aComponents)
		{
			foreach (EPF_PersistentComponentSaveData persistentComp : characterData.m_aComponents)
			{
				if (!persistentComp || !persistentComp.m_pData)
					continue;

				// Level
				EL_PlayerLevelComponentSaveData levelData = EL_PlayerLevelComponentSaveData.Cast(persistentComp.m_pData);
				if (levelData)
				{
					level = levelData.m_iPlayerLevel;
					continue;
				}

				// Bank
				EL_BankAccountComponentSaveData bankData = EL_BankAccountComponentSaveData.Cast(persistentComp.m_pData);
				if (bankData)
				{
					money = bankData.m_iAccountBalance;
					continue;
				}

				// Job
				EL_PlayerJobComponentSaveData jobData = EL_PlayerJobComponentSaveData.Cast(persistentComp.m_pData);
				if (jobData)
				{
					jobName = GetJobNameFromType(jobData.m_eCurrentJob);
					continue;
				}

				// Group
				EL_PlayerGroupComponentSaveData groupData = EL_PlayerGroupComponentSaveData.Cast(persistentComp.m_pData);
				if (groupData && !groupData.m_sGroupId.IsEmpty())
				{
					EL_GroupsRegistry groupsRegistry = EL_GroupsRegistry.GetInstance();
					if (groupsRegistry)
					{
						EL_GroupData group = groupsRegistry.GetGroup(groupData.m_sGroupId);
						if (group)
							groupName = group.m_sGroupName;
					}
				}

				// Apartment
				EL_ApartmentPlayerComponentSaveData aptData = EL_ApartmentPlayerComponentSaveData.Cast(persistentComp.m_pData);
				if (aptData)
				{
					hasApartment = aptData.m_bOwnsApartment;
					continue;
				}
			}

			// Vehicles - from GarageRegistry using character ID
			EL_GarageRegistry garageRegistry = EL_GarageRegistry.GetInstance();
			if (garageRegistry)
			{
				array<ref EL_VehicleSaveData> vehicles = garageRegistry.GetVehiclesByOwner(requestContext.m_sCharacterId);
				if (vehicles)
					vehicleCount = vehicles.Count();
			}
		}

		Rpc(RPC_ReceiveCharacterStats_Owner, requestContext.m_sCharacterId, requestContext.m_iRequestId, level, money, jobName, groupName, vehicleCount, hasApartment);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: recibe stats del personaje y los pasa al menu
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ReceiveCharacterStats_Owner(string characterId, int requestId, int level, int money, string jobName, string groupName, int vehicleCount, bool hasApartment)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MenuBase currentMenu = menuManager.GetTopMenu();
		if (!currentMenu)
			return;

		EL_CharacterSelectionMenu selectionMenu = EL_CharacterSelectionMenu.Cast(currentMenu);
		if (!selectionMenu)
			return;

		selectionMenu.OnCharacterStatsReceived(characterId, requestId, level, money, jobName, groupName, vehicleCount, hasApartment);
	}

	//------------------------------------------------------------------------------------------------
	//! Helper: convertir EL_EJobType a nombre legible
	protected static string GetJobNameFromType(EL_EJobType job)
	{
		switch (job)
		{
			case EL_EJobType.UNEMPLOYED: return WidgetManager.Translate("#DH-Job_Unemployed");
			case EL_EJobType.FARMER: return WidgetManager.Translate("#DH-Job_Farmer");
			case EL_EJobType.MINER: return WidgetManager.Translate("#DH-Job_Miner");
			case EL_EJobType.LUMBERJACK: return WidgetManager.Translate("#DH-Job_Lumberjack");
			case EL_EJobType.MEDIC: return WidgetManager.Translate("#DH-Job_Medic");
			case EL_EJobType.POLICE: return WidgetManager.Translate("#DH-Job_Police");
			case EL_EJobType.MAFIA: return WidgetManager.Translate("#DH-Job_Mafia");
		}
		return WidgetManager.Translate("#DH-Job_Unknown");
	}

	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el SERVIDOR para borrar personaje del account y liberar ranura
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_DeleteCharacter_Server(string characterId)
	{
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: Owner is not a PlayerController!", LogLevel.ERROR);
			return;
		}

		int playerId = playerController.GetPlayerId();
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);

		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerUID);
		if (!account)
		{
			Print(string.Format("[EL_UIRPCComponent] ERROR: No account found while deleting character for player %1", playerUID), LogLevel.ERROR);
			return;
		}

		EL_PlayerCharacter character = account.GetCharacterByUID(characterId);
		if (!character)
		{
			Print(string.Format("[EL_UIRPCComponent] WARNING: Character %1 not found for deletion", characterId), LogLevel.WARNING);
			return;
		}

		account.RemoveCharacter(character);

		int countAfterDelete = account.GetCharacterCount();
		if (countAfterDelete > 0)
		{
			int currentActiveIdx = account.GetActiveCharacterIndex();
			if (currentActiveIdx < 0 || currentActiveIdx >= countAfterDelete)
				account.SetActiveCharacterIndex(0);
		}

		account.SafeSave();
		Print(string.Format("[EL_UIRPCComponent] Character deleted for player %1 (characterId=%2). Remaining=%3", playerUID, characterId, countAfterDelete), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el SERVIDOR cuando un jugador selecciona un character
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_SelectCharacter_Server(string characterId)
	{
		//Print(string.Format("[EL_UIRPCComponent] RPC received on SERVER - Selecting character %1", characterId), LogLevel.NORMAL);
		
		// Obtener el PlayerController owner de este componente
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: Owner is not a PlayerController!", LogLevel.ERROR);
			return;
		}
		
		int playerId = playerController.GetPlayerId();
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);
		
		// Cargar el account
		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerUID);
		if (!account)
		{
			Print(string.Format("[EL_UIRPCComponent] ERROR: No account found for player %1", playerUID), LogLevel.ERROR);
			return;
		}
		
		// Cargar el character seleccionado
		EL_PlayerCharacter character = account.GetCharacterByUID(characterId);
		if (!character)
		{
			Print(string.Format("[EL_UIRPCComponent] ERROR: Character %1 not found in account!", characterId), LogLevel.ERROR);
			return;
		}
		
		// Establecer como character activo
		account.SetActiveCharacter(character);
		account.SafeSave();
		
		// Si el character muriÃ³ en la Ãºltima sesiÃ³n, abrir menÃº de spawn
		if (character.WasDeadOnDisconnect())
		{
			//Print(string.Format("[EL_UIRPCComponent] Character %1 was dead - opening spawn selection menu", characterId), LogLevel.NORMAL);
			
			// Obtener datos primitivos del character
			string firstName = character.GetFirstName();
			string lastName = character.GetLastName();
			int age = character.GetAge();
			ResourceName prefab = character.GetPrefab();
			
			// Enviar RPC al cliente para abrir el menÃº de spawn
			Rpc(RPC_OpenSpawnSelectionMenu_Owner, playerId, playerUID, characterId, firstName, lastName, age, prefab);
		}
		else
		{
			// El character estaba vivo - spawnear en su Ãºltima posiciÃ³n
			//Print(string.Format("[EL_UIRPCComponent] Character %1 was alive - spawning at last position", characterId), LogLevel.NORMAL);
			
			vector lastPosition = character.GetLastSpawnPosition();
			vector lastRotation = character.GetLastSpawnRotation();
			ResourceName prefab = character.GetPrefab();
			
			// Obtener el spawn logic
			SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.Cast(
				GetGame().GetGameMode().FindComponent(SCR_RespawnSystemComponent)
			);
			
			if (!respawnSystem)
			{
				Print("[EL_UIRPCComponent] ERROR: No respawn system found!", LogLevel.ERROR);
				return;
			}
			
			EL_SpawnLogic spawnLogic = EL_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());
			if (!spawnLogic)
			{
				Print("[EL_UIRPCComponent] ERROR: No spawn logic found!", LogLevel.ERROR);
				return;
			}
			
			// wasAlive=true porque el character estaba vivo en la Ãºltima sesiÃ³n
			spawnLogic.CreatePlayerCharacter(playerId, characterId, prefab, lastPosition, lastRotation, true);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar RPC al cliente para sincronizar el nombre del personaje en el HUD
	//! Se llama desde el SERVIDOR despuÃ©s de establecer el nombre
	void RPC_SyncCharacterName(string characterName)
	{
		//Print(string.Format("[EL_UIRPCComponent] SERVER sending character name to client: '%1'", characterName), LogLevel.NORMAL);
		Rpc(RPC_SyncCharacterName_Owner, characterName);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el CLIENTE para actualizar el nombre del personaje en el HUD
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_SyncCharacterName_Owner(string characterName)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT received character name: '%1'", characterName), LogLevel.NORMAL);
		
		// Buscar el HUD y actualizar el nombre
		PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!playerController) 
		{
			Print("[EL_UIRPCComponent] ERROR: No player controller found!", LogLevel.ERROR);
			return;
		}
		
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.Cast(playerController.FindComponent(SCR_HUDManagerComponent));
		if (!hudManager)
		{
			Print("[EL_UIRPCComponent] ERROR: No HUD manager found!", LogLevel.ERROR);
			return;
		}
		
		EL_BetaHud betaHud = EL_BetaHud.Cast(hudManager.FindInfoDisplay(EL_BetaHud));
		if (betaHud)
		{
			betaHud.SetCharacterNameDirect(characterName);
			//Print(string.Format("[EL_UIRPCComponent] âœ“ Character name set in BetaHud: '%1'", characterName), LogLevel.NORMAL);
		}
		
		EL_CircularHud circularHud = EL_CircularHud.Cast(hudManager.FindInfoDisplay(EL_CircularHud));
		if (circularHud)
		{
			circularHud.SetCharacterNameDirect(characterName);
			//Print(string.Format("[EL_UIRPCComponent] âœ“ Character name set in CircularHud: '%1'", characterName), LogLevel.NORMAL);
		}
		
		if (!betaHud && !circularHud)
		{
			Print("[EL_UIRPCComponent] ERROR: No HUD found for name setting!", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Enviar notificaciÃ³n al cliente (owner de este componente)
	void RPC_ShowNotification(string title, string message, float duration, int notificationType)
	{
		if (!Replication.IsServer())
			return;
			
		//Print(string.Format("[EL_UIRPCComponent] SERVER sending notification RPC: '%1'", title), LogLevel.NORMAL);
		Rpc(RPC_ShowNotification_Owner, title, message, duration, notificationType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Recibir notificaciÃ³n y mostrarla
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ShowNotification_Owner(string title, string message, float duration, int notificationType)
	{
		//Print(string.Format("[EL_UIRPCComponent] ðŸ“¨ CLIENT RPC received notification: '%1'", title), LogLevel.NORMAL);
		
		// Solo ejecutar en cliente
		if (Replication.IsServer())
		{
			Print("[EL_UIRPCComponent] â­ï¸ Skipping - running on server", LogLevel.VERBOSE);
			return;
		}
		
		// Obtener el NotificationManager y mostrar el toast
		EL_NotificationManagerComponent notifManager = EL_NotificationManagerComponent.GetInstance();
		if (notifManager)
		{
			EL_ENotificationType type = notificationType;
			notifManager.ShowNotificationToast(title, message, duration, type);
			//Print(string.Format("[EL_UIRPCComponent] âœ“ Notification displayed: '%1'", title), LogLevel.NORMAL);
		}
		else
		{
			Print("[EL_UIRPCComponent] ERROR: NotificationManager not found!", LogLevel.ERROR);
		}
	}
	
	// =====================================================================
	//  CHARACTER SYNC (radial menu save request)
	// =====================================================================

	//! CLIENT: Request the server to persist the player's character
	void RequestSyncCharacter()
	{
		if (Replication.IsServer())
		{
			DoSyncCharacter();
			return;
		}

		Rpc(RPC_RequestSyncCharacter_Server);
	}

	//! SERVER: Persist the caller's controlled entity and notify the client
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestSyncCharacter_Server()
	{
		DoSyncCharacter();
	}

	protected void DoSyncCharacter()
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(pc.GetPlayerId());
		if (!character)
		{
			Print("[EL_UIRPCComponent] SyncCharacter: no controlled entity", LogLevel.WARNING);
			return;
		}

		EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(character);
		if (!persistence || persistence.IsTemporaryNonPersistent())
		{
			Print("[EL_UIRPCComponent] SyncCharacter: entity not persistable", LogLevel.WARNING);
			return;
		}

		persistence.Save();
		Print(string.Format("[EL_UIRPCComponent] SyncCharacter: saved player %1", pc.GetPlayerId()), LogLevel.NORMAL);

		// Confirm to client
		Rpc(RPC_SyncCharacterConfirm_Owner);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_SyncCharacterConfirm_Owner()
	{
		SCR_HintManagerComponent.ShowCustomHint(WidgetManager.Translate("#DH-Sync_CharSynced"), WidgetManager.Translate("#DH-Sync_Title"), 3.0);
	}

	// =====================================================================
	//  EMS REQUEST FROM DEATH MENU (ruta PlayerController — personaje muerto)
	// =====================================================================
	
	//! CLIENT: Solicitar EMS desde el menú de muerte (vía PlayerController, NO vía personaje muerto)
	void RequestEMSFromDeath()
	{
		if (Replication.IsServer())
			return;
		
		Rpc(RPC_RequestEMSFromDeath_Server);
	}
	
	//! SERVER: Procesar solicitud EMS recibida desde cliente muerto
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestEMSFromDeath_Server()
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
		{
			Print("[EL_UIRPCComponent] RPC_RequestEMSFromDeath_Server: PlayerController not found", LogLevel.ERROR);
			return;
		}
		
		int callerPlayerId = pc.GetPlayerId();
		IEntity callerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(callerPlayerId);
		if (!callerEntity)
		{
			Print("[EL_UIRPCComponent] RPC_RequestEMSFromDeath_Server: Caller entity not found", LogLevel.ERROR);
			return;
		}
		
		// 1. Notificación toast a todos los médicos en servicio
		EL_NotificationManagerComponent.NotifyJob(
			EL_EJobType.MEDIC,
			WidgetManager.Translate("#DH-EMS_AlertTitle"),
			WidgetManager.Translate("#DH-EMS_AlertBody"),
			10.0,
			EL_ENotificationType.EMS_ALERT,
			true
		);
		
		// 2. Crear entrada en PDA EMS
		string callerName = EL_Utils.GetCharacterName(SCR_ChimeraCharacter.Cast(callerEntity));
		vector pos = callerEntity.GetOrigin();
		EL_RpcSenderComponent.EMSPDA_CreateCallFromDeath(callerName, callerPlayerId, pos[0], pos[2]);
		
		Print(string.Format("[EL_UIRPCComponent] EMS request processed for player %1 (%2) at (%3, %4)", 
			callerPlayerId, callerName, pos[0], pos[2]), LogLevel.NORMAL);
	}
	
	// =====================================================================
	//  EMS CALLER NOTIFICATION (servidor → cliente incapacitado)
	// =====================================================================
	
	//! SERVER: Enviar notificación EMS al jugador (funciona incluso si está incapacitado)
	void SendEMSCallerNotification(string message)
	{
		if (!Replication.IsServer())
			return;
		
		Rpc(RPC_EMSCallerNotification_Owner, message);
	}
	
	//! CLIENT: Recibir notificación de respuesta EMS
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_EMSCallerNotification_Owner(string message)
	{
		if (Replication.IsServer())
			return;
		
		EL_EMSPDAMenuUI.ShowCallerNotification(message);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request mission data from server
	void RPC_RequestMissionData()
	{
		if (Replication.IsServer())
			return;
		
		//Print("[EL_UIRPCComponent] CLIENT requesting mission data from server", LogLevel.NORMAL);
		Rpc(RPC_RequestMissionData_Server);
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Receive mission data request and send response
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestMissionData_Server()
	{
		//Print("[EL_UIRPCComponent] SERVER received mission data request", LogLevel.NORMAL);
		
		// Get mission manager from GameMode
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
		{
			Print("[EL_UIRPCComponent] ERROR: GameMode not found", LogLevel.ERROR);
			return;
		}
		
		EL_MissionManagerComponent missionManager = EL_MissionManagerComponent.Cast(gameMode.FindComponent(EL_MissionManagerComponent));
		if (!missionManager)
		{
			Print("[EL_UIRPCComponent] ERROR: MissionManager not found", LogLevel.ERROR);
			return;
		}
		
		// Get player mission component for active/completed missions
		// EL_PlayerMissionComponent is on the CHARACTER entity, not the PlayerController
		PlayerController _missionPc = PlayerController.Cast(GetOwner());
		IEntity _missionChar = null;
		if (_missionPc)
			_missionChar = _missionPc.GetControlledEntity();
		EL_PlayerMissionComponent playerMissions = null;
		if (_missionChar)
			playerMissions = EL_PlayerMissionComponent.Cast(_missionChar.FindComponent(EL_PlayerMissionComponent));
		
		// Collect all mission data
		array<string> allMissionIds = missionManager.GetAllMissionIds();
		array<string> missionNames = {};
		array<string> missionDescs = {};
		array<int> missionTypes = {};
		array<int> missionRewards = {};
		array<int> missionXpRewards = {};
		array<string> activeMissions = {};
		array<string> completedMissions = {};
		
		foreach (string id : allMissionIds)
		{
			EL_MissionConfig config = missionManager.GetMissionConfig(id);
			if (config)
			{
				missionNames.Insert(config.m_sName);
				missionDescs.Insert(config.m_sDescription);
				missionTypes.Insert(config.m_eType);
				missionRewards.Insert(config.m_iMoneyReward);
				missionXpRewards.Insert(config.m_fXPReward);
			}
		}
		
		if (playerMissions)
		{
			activeMissions = playerMissions.GetActiveMissions();
			completedMissions = playerMissions.GetCompletedMissions();
		}
		
		// âœ… Recopilar objetivos de misiones activas
		array<string> objectiveMissionIds = {};
		array<int> objectiveTypes = {};
		array<string> objectiveDescs = {};
		array<float> objectiveCurrents = {};
		array<float> objectiveRequireds = {};
		string trackedMissionId = "";
		
		if (playerMissions)
		{
			trackedMissionId = playerMissions.GetTrackedMissionId();
			
			foreach (string activeMissionId : activeMissions)
			{
				EL_PlayerMissionState missionState = playerMissions.GetMissionState(activeMissionId);
				if (missionState && missionState.m_aObjectives)
				{
					foreach (EL_MissionObjective obj : missionState.m_aObjectives)
					{
						objectiveMissionIds.Insert(activeMissionId);
						objectiveTypes.Insert(obj.m_eType);
						objectiveDescs.Insert(obj.m_sDescription);
						objectiveCurrents.Insert(obj.m_fCurrentProgress);
						objectiveRequireds.Insert(obj.m_fRequiredProgress);
					}
				}
			}
		}
		
		//Print(string.Format("[EL_UIRPCComponent] SERVER sending %1 missions, %2 objectives, tracked: %3 to client", allMissionIds.Count(), objectiveMissionIds.Count(), trackedMissionId), LogLevel.NORMAL);
		
		// Send RPC split in 2 calls to avoid parameter limit (max 8 params per Rpc call)
		Rpc(RPC_ReceiveMissionData_Owner, allMissionIds, missionNames, missionDescs, missionTypes, missionRewards, activeMissions, completedMissions, trackedMissionId);
		Rpc(RPC_ReceiveMissionObjectives_Owner, objectiveMissionIds, objectiveTypes, objectiveDescs, objectiveCurrents, objectiveRequireds, missionXpRewards);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Receive mission data from server (basic data)
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ReceiveMissionData_Owner(array<string> missionIds, array<string> names, array<string> descs, array<int> types, array<int> rewards, array<string> active, array<string> completed, string trackedMissionId)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT received %1 missions, tracked: %2 from server", missionIds.Count(), trackedMissionId), LogLevel.NORMAL);
		
		// Find and update the mission menu if it's open
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;
		
		MenuBase currentMenu = menuManager.GetTopMenu();
		if (!currentMenu)
			return;
		
		EL_MissionMenuUI missionMenu = EL_MissionMenuUI.Cast(currentMenu);
		if (missionMenu)
		{
			missionMenu.OnMissionDataReceived(missionIds, names, descs, types, rewards, active, completed, trackedMissionId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Receive mission objectives from server (second part)
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ReceiveMissionObjectives_Owner(array<string> objMissionIds, array<int> objTypes, array<string> objDescs, array<float> objCurrents, array<float> objRequireds, array<int> xpRewards)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT received %1 objectives from server", objMissionIds.Count()), LogLevel.NORMAL);
		
		// Find and update the mission menu if it's open
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;
		
		MenuBase currentMenu = menuManager.GetTopMenu();
		if (!currentMenu)
			return;
		
		EL_MissionMenuUI missionMenu = EL_MissionMenuUI.Cast(currentMenu);
		if (missionMenu)
		{
			missionMenu.OnMissionObjectivesReceived(objMissionIds, objTypes, objDescs, objCurrents, objRequireds, xpRewards);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVERâ†’CLIENT: Notify that mission data changed, request refresh
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RPC_NotifyMissionMenuUpdate_Owner()
	{
		//Print("[EL_UIRPCComponent] CLIENT received mission menu update notification", LogLevel.NORMAL);
		
		// Find mission menu if it's open
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;
		
		MenuBase currentMenu = menuManager.GetTopMenu();
		if (!currentMenu)
			return;
		
		EL_MissionMenuUI missionMenu = EL_MissionMenuUI.Cast(currentMenu);
		if (missionMenu)
		{
			//Print("[EL_UIRPCComponent] Mission menu is open, requesting updated data", LogLevel.NORMAL);
			RPC_RequestMissionData(); // Re-request data from server
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request university data from server
	void RPC_RequestUniversityData()
	{
		if (Replication.IsServer())
			return;
		
		//Print("[EL_UIRPCComponent] CLIENT requesting university data from server", LogLevel.NORMAL);
		Rpc(RPC_RequestUniversityData_Server);
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Receive university data request and send response
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestUniversityData_Server()
	{
		//Print("[EL_UIRPCComponent] SERVER received university data request", LogLevel.NORMAL);

		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: GetOwner() is not a PlayerController", LogLevel.ERROR);
			return;
		}
		IEntity owner = playerController.GetControlledEntity();
		if (!owner)
		{
			Print("[EL_UIRPCComponent] ERROR: PlayerController has no controlled entity", LogLevel.ERROR);
			return;
		}

		// Get player components
		EL_LicenseManagerComponent licenseManager = EL_LicenseManagerComponent.Cast(owner.FindComponent(EL_LicenseManagerComponent));
		EL_PlayerLevelComponent playerLevel = EL_PlayerLevelComponent.Cast(owner.FindComponent(EL_PlayerLevelComponent));

		if (!licenseManager || !playerLevel)
		{
			Print("[EL_UIRPCComponent] ERROR: Player components not found on character entity", LogLevel.ERROR);
			return;
		}
		
		// Collect license data
		array<int> allLicenseTypes = {};
		array<string> licenseNames = {};
		array<string> licenseDescs = {};
		array<int> licenseCosts = {};
		array<int> licenseCashCosts = {};
		array<int> licenseRequiredLevels = {};
		array<int> licenseEnergyCosts = {};
		array<bool> licenseUnlocked = {};
		array<int> licenseMeta = {};
		
		// Obtener todas las configuraciones del LicenseManager
		map<EL_ELicenseType, ref EL_LicenseConfig> allConfigs = licenseManager.GetAllLicenseConfigs();
		
		// Iterar sobre todas las licencias configuradas
		foreach (EL_ELicenseType licenseType, EL_LicenseConfig config : allConfigs)
		{
			allLicenseTypes.Insert(licenseType);
			licenseNames.Insert(config.m_sLicenseName);
			licenseDescs.Insert(config.m_sDescription);
			licenseCosts.Insert(config.m_iSkillPointCost);
			licenseCashCosts.Insert(config.m_iCashCost);
			licenseRequiredLevels.Insert(config.m_iMinLevelRequired);
			licenseEnergyCosts.Insert(config.m_iEnergyCost);
			licenseUnlocked.Insert(licenseManager.HasLicense(licenseType));
			licenseMeta.Insert(config.m_iSkillPointCost);
			licenseMeta.Insert(config.m_iCashCost);
			licenseMeta.Insert(config.m_iMinLevelRequired);
			licenseMeta.Insert(config.m_iEnergyCost);
			licenseMeta.Insert(licenseManager.HasLicense(licenseType));
		}
		
		int currentLevel = playerLevel.GetLevel();
		int currentSP = playerLevel.GetSkillPoints();
		int playerCash = EL_MoneyUtils.GetCash(owner);
		int playerEnergy = playerLevel.GetCurrentEnergy();
		array<int> playerStats = {currentLevel, currentSP, playerCash, playerEnergy};
		
		Print(string.Format("[EL_UIRPCComponent] SERVER sending %1 licenses to client (Level: %2, SP: %3, Cash: %4, EN: %5)", 
			allLicenseTypes.Count(), currentLevel, currentSP, playerCash, playerEnergy), LogLevel.NORMAL);
		
		// Send RPC with university data
		Rpc(RPC_ReceiveUniversityData_Owner, allLicenseTypes, licenseNames, licenseDescs, licenseMeta, playerStats);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Receive university data from server
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ReceiveUniversityData_Owner(array<int> licenseTypes, array<string> names, array<string> descs, 
		array<int> licenseMeta, array<int> playerStats)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT received %1 licenses from server", licenseTypes.Count()), LogLevel.NORMAL);
		
		// Find and update the university menu if it's open
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;
		
		MenuBase currentMenu = menuManager.GetTopMenu();
		if (!currentMenu)
			return;
		
		EL_UniversityMenu universityMenu = EL_UniversityMenu.Cast(currentMenu);
		if (universityMenu)
		{
			int playerLevel = 0;
			int playerSP = 0;
			int playerCash = 0;
			int playerEnergy = 0;
			if (playerStats)
			{
				if (playerStats.Count() > 0) playerLevel = playerStats[0];
				if (playerStats.Count() > 1) playerSP = playerStats[1];
				if (playerStats.Count() > 2) playerCash = playerStats[2];
				if (playerStats.Count() > 3) playerEnergy = playerStats[3];
			}

			array<int> costs = {};
			array<int> cashCosts = {};
			array<int> requiredLevels = {};
			array<int> energyCosts = {};
			array<bool> unlocked = {};
			if (licenseMeta)
			{
				for (int i = 0; i + 4 < licenseMeta.Count(); i += 5)
				{
					costs.Insert(licenseMeta[i]);
					cashCosts.Insert(licenseMeta[i + 1]);
					requiredLevels.Insert(licenseMeta[i + 2]);
					energyCosts.Insert(licenseMeta[i + 3]);
					unlocked.Insert(licenseMeta[i + 4] != 0);
				}
			}

			universityMenu.OnUniversityDataReceived(licenseTypes, names, descs, costs, cashCosts, requiredLevels, 
				energyCosts, unlocked, playerLevel, playerSP, playerCash, playerEnergy);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Open a menu on client (generic)
	void RPC_OpenMenuByPreset(ChimeraMenuPreset preset)
	{
		if (!Replication.IsServer())
			return;
			
		//Print(string.Format("[EL_UIRPCComponent] SERVER sending RPC to open menu preset %1", preset), LogLevel.NORMAL);
		Rpc(RPC_OpenMenuByPreset_Owner, preset);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Receive menu open request
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenMenuByPreset_Owner(ChimeraMenuPreset preset)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT opening menu preset %1", preset), LogLevel.NORMAL);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;
			
		menuManager.OpenMenu(preset);
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Open Mining Warehouse menu with data
	void RPC_OpenMiningWarehouseMenu(RplId warehouseRplId, string warehouseName, array<int> resourceTypes, array<int> currentStocks, array<int> maxStocks)
	{
		if (!Replication.IsServer())
			return;
			
		//Print(string.Format("[EL_UIRPCComponent] SERVER sending RPC to open mining warehouse menu: %1", warehouseName), LogLevel.NORMAL);
		Rpc(RPC_OpenMiningWarehouseMenu_Owner, warehouseRplId, warehouseName, resourceTypes, currentStocks, maxStocks);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Open mining warehouse menu with data
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenMiningWarehouseMenu_Owner(RplId warehouseRplId, string warehouseName, array<int> resourceTypes, array<int> currentStocks, array<int> maxStocks)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT opening mining warehouse menu: %1", warehouseName), LogLevel.NORMAL);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;
		
		// Open menu
		menuManager.OpenMenu(ChimeraMenuPreset.EL_MiningWarehouseMenu);

		// Get player character entity â€” GetOwner() returns PlayerController, not character
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
			return;
		IEntity playerEntity = playerController.GetControlledEntity();
		if (!playerEntity)
			return;
		
		// Find warehouse entity by RplId
		RplComponent warehouseRpl = RplComponent.Cast(Replication.FindItem(warehouseRplId));
		if (!warehouseRpl)
			return;
		
		IEntity warehouseEntity = warehouseRpl.GetEntity();
		if (!warehouseEntity)
			return;
		
		EL_MiningWarehouseComponent warehouse = EL_MiningWarehouseComponent.Cast(warehouseEntity.FindComponent(EL_MiningWarehouseComponent));
		if (!warehouse)
			return;
		
		// Aplicar stock recibido via RPC al componente local
		map<EL_EMiningResourceType, ref EL_MiningWarehouseStock> localStocks = warehouse.GetAllStock();
		if (localStocks && resourceTypes && currentStocks && maxStocks)
		{
			int count = Math.Min(resourceTypes.Count(), Math.Min(currentStocks.Count(), maxStocks.Count()));
			for (int i = 0; i < count; i++)
			{
				EL_EMiningResourceType resType = resourceTypes[i];
				EL_MiningWarehouseStock stock = localStocks.Get(resType);
				if (stock)
				{
					stock.m_iCurrentStock = currentStocks[i];
					stock.m_iMaxStock = maxStocks[i];
				}
			}
			//Print(string.Format("[EL_UIRPCComponent] Applied %1 stock entries to local warehouse", count), LogLevel.NORMAL);
		}
		
		// Set data on menu
		EL_MiningWarehouseMenuDialogUI menuDialog = EL_MiningWarehouseMenuDialogUI.Cast(menuManager.GetTopMenu());
		if (menuDialog)
		{
			menuDialog.SetPlayerAndMiningWarehouse(playerEntity, warehouse);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Open regular Warehouse menu with data
	void RPC_OpenWarehouseMenu(RplId warehouseRplId, string warehouseName, array<int> resourceTypes, array<int> currentStocks, array<int> maxStocks)
	{
		if (!Replication.IsServer())
			return;
			
		//Print(string.Format("[EL_UIRPCComponent] SERVER sending RPC to open warehouse menu: %1", warehouseName), LogLevel.NORMAL);
		Rpc(RPC_OpenWarehouseMenu_Owner, warehouseRplId, warehouseName, resourceTypes, currentStocks, maxStocks);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Open warehouse menu with data
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenWarehouseMenu_Owner(RplId warehouseRplId, string warehouseName, array<int> resourceTypes, array<int> currentStocks, array<int> maxStocks)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT opening warehouse menu: %1", warehouseName), LogLevel.NORMAL);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;
		
		// Open menu
		menuManager.OpenMenu(ChimeraMenuPreset.EL_WarehouseMenu);

		// Get player character entity â€” GetOwner() returns PlayerController, not character
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
			return;
		IEntity playerEntity = playerController.GetControlledEntity();
		if (!playerEntity)
			return;
		
		// Find warehouse entity by RplId
		RplComponent warehouseRpl = RplComponent.Cast(Replication.FindItem(warehouseRplId));
		if (!warehouseRpl)
			return;
		
		IEntity warehouseEntity = warehouseRpl.GetEntity();
		if (!warehouseEntity)
			return;
		
		EL_WarehouseComponent warehouse = EL_WarehouseComponent.Cast(warehouseEntity.FindComponent(EL_WarehouseComponent));
		if (!warehouse)
			return;
		
		// Aplicar stock recibido via RPC al componente local (usa m_mStock)
		// Necesitamos acceso al map interno - vamos a recrearlo basado en configs
		if (resourceTypes && currentStocks && maxStocks)
		{
			int count = Math.Min(resourceTypes.Count(), Math.Min(currentStocks.Count(), maxStocks.Count()));
			for (int i = 0; i < count; i++)
			{
				EL_EResourceType resType = resourceTypes[i];
				EL_WarehouseStock stock = warehouse.GetStock(resType);
				if (stock)
				{
					stock.m_iCurrentStock = currentStocks[i];
					stock.m_iMaxStock = maxStocks[i];
				}
			}
			//Print(string.Format("[EL_UIRPCComponent] Applied %1 stock entries to local warehouse", count), LogLevel.NORMAL);
		}
		
		// Set data on menu
		EL_WarehouseMenuUI menu = EL_WarehouseMenuUI.Cast(menuManager.GetTopMenu());
		if (menu)
		{
			menu.SetPlayerAndWarehouse(playerEntity, warehouse);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Open Gas Station menu with data
	void RPC_OpenGasStationMenu(RplId gasStationRplId, RplId vehicleRplId)
	{
		if (!Replication.IsServer())
			return;
			
		//Print(string.Format("[EL_UIRPCComponent] SERVER sending RPC to open gas station menu"), LogLevel.NORMAL);
		Rpc(RPC_OpenGasStationMenu_Owner, gasStationRplId, vehicleRplId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Open gas station menu with data
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenGasStationMenu_Owner(RplId gasStationRplId, RplId vehicleRplId)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT opening gas station menu"), LogLevel.NORMAL);
		
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;
		
		// Get player character entity (GetOwner() on EL_UIRPCComponent returns PlayerController)
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;
		IEntity playerEntity = pc.GetControlledEntity();
		if (!playerEntity)
			return;
		
		// Find gas station entity by RplId
		RplComponent gasStationRpl = RplComponent.Cast(Replication.FindItem(gasStationRplId));
		IEntity gasStationEntity = null;
		if (gasStationRpl)
			gasStationEntity = gasStationRpl.GetEntity();
		
		// Find vehicle entity by RplId
		RplComponent vehicleRpl = RplComponent.Cast(Replication.FindItem(vehicleRplId));
		IEntity vehicleEntity = null;
		if (vehicleRpl)
			vehicleEntity = vehicleRpl.GetEntity();
		
		// Open menu
		MenuBase menuBase = menuManager.OpenMenu(ChimeraMenuPreset.EL_GasStationMenu);
		EL_GasStationMenuUI menu = EL_GasStationMenuUI.Cast(menuBase);
		
		if (menu)
		{
			menu.SetGasStationAndVehicle(gasStationEntity, vehicleEntity, playerEntity);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request server to process refueling payment and apply fuel to vehicle
	void RPC_RequestProcessRefueling(RplId vehicleRplId, float liters, int totalCost, bool useBank)
	{
		if (Replication.IsServer())
			return;
		Rpc(RPC_ProcessRefueling_Server, vehicleRplId, liters, totalCost, useBank);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: Validate payment and apply fuel to vehicle
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_ProcessRefueling_Server(RplId vehicleRplId, float liters, int totalCost, bool useBank)
	{
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController) return;
		IEntity playerEntity = playerController.GetControlledEntity();
		if (!playerEntity) return;

		// Server-side validation: recalculate max cost to prevent client tampering
		if (liters <= 0 || totalCost <= 0)
			return;

		if (useBank)
		{
			EL_BankAccountComponent bankAccount = EL_BankAccountComponent.Cast(playerEntity.FindComponent(EL_BankAccountComponent));
			if (!bankAccount || bankAccount.GetBalance() < totalCost)
			{
				Rpc(RPC_RefuelingResult_Owner, false, "Saldo bancario insuficiente");
				return;
			}
			bankAccount.Withdraw(totalCost, "Repostaje gasolinera");
		}
		else
		{
			if (EL_MoneyUtils.GetCash(playerEntity) < totalCost)
			{
				Rpc(RPC_RefuelingResult_Owner, false, "Efectivo insuficiente");
				return;
			}
			EL_MoneyUtils.TakeCash(playerEntity, totalCost);
		}

		RplComponent vehicleRpl = RplComponent.Cast(Replication.FindItem(vehicleRplId));
		if (vehicleRpl)
		{
			IEntity vehicleEntity = vehicleRpl.GetEntity();
			if (vehicleEntity)
			{
				FuelManagerComponent fuelManager = FuelManagerComponent.Cast(vehicleEntity.FindComponent(FuelManagerComponent));
				if (fuelManager)
				{
					array<BaseFuelNode> fuelNodes = {};
					fuelManager.GetFuelNodesList(fuelNodes);
					if (!fuelNodes.IsEmpty())
					{
						BaseFuelNode node = fuelNodes[0];
						float newFuel = Math.Min(node.GetFuel() + liters, node.GetMaxFuel());
						node.SetFuel(newFuel);
						Print(string.Format("[EL_UIRPCComponent] Refueled vehicle: +%1L (now %2/%3), cost $%4", liters, newFuel, node.GetMaxFuel(), totalCost), LogLevel.NORMAL);
					}
				}
			}
		}

		Rpc(RPC_RefuelingResult_Owner, true, "");
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Receive refueling result (shows error if failed)
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_RefuelingResult_Owner(bool success, string errorMsg)
	{
		if (!success && !errorMsg.IsEmpty())
			SCR_HintManagerComponent.ShowCustomHint(errorMsg, WidgetManager.Translate("#DH-Gas_RefuelError"), 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request purchase of a university license
	void RPC_RequestPurchaseLicense(EL_ELicenseType licenseType)
	{
		if (Replication.IsServer())
			return;
			
		//Print(string.Format("[EL_UIRPCComponent] CLIENT requesting to purchase license: %1", typename.EnumToString(EL_ELicenseType, licenseType)), LogLevel.NORMAL);
		Rpc(RPC_RequestPurchaseLicense_Server, licenseType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Process license purchase request
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestPurchaseLicense_Server(EL_ELicenseType licenseType)
	{
		//Print(string.Format("[EL_UIRPCComponent] SERVER received purchase request for license: %1", typename.EnumToString(EL_ELicenseType, licenseType)), LogLevel.NORMAL);

		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: GetOwner() is not a PlayerController", LogLevel.ERROR);
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, "Error interno del servidor");
			return;
		}

		IEntity owner = playerController.GetControlledEntity();
		if (!owner)
		{
			Print("[EL_UIRPCComponent] ERROR: PlayerController has no controlled entity", LogLevel.ERROR);
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, "Error interno del servidor");
			return;
		}

		// Get player components from character entity
		EL_LicenseManagerComponent licenseManager = EL_LicenseManagerComponent.Cast(owner.FindComponent(EL_LicenseManagerComponent));
		EL_PlayerLevelComponent playerLevel = EL_PlayerLevelComponent.Cast(owner.FindComponent(EL_PlayerLevelComponent));
		
		if (!licenseManager || !playerLevel)
		{
			Print("[EL_UIRPCComponent] ERROR: Player components not found for license purchase", LogLevel.ERROR);
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, "Error interno del servidor");
			return;
		}
		
		// Check if already has license
		if (licenseManager.HasLicense(licenseType))
		{
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, "Ya posees esta licencia");
			return;
		}
		
		// Get license config
		EL_LicenseConfig config = licenseManager.GetLicenseConfig(licenseType);
		if (!config)
		{
			Print("[EL_UIRPCComponent] ERROR: License config not found!", LogLevel.ERROR);
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, "Licencia no encontrada");
			return;
		}
		
		// Check level requirement
		if (playerLevel.GetPlayerLevel() < config.m_iMinLevelRequired)
		{
			string message = string.Format("Requieres nivel %1 (Tu nivel: %2)", config.m_iMinLevelRequired, playerLevel.GetPlayerLevel());
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, message);
			return;
		}
		
		// Check SP requirement
		if (playerLevel.GetSkillPoints() < config.m_iSkillPointCost)
		{
			string message = string.Format("Requieres %1 SP (Tienes: %2 SP)", config.m_iSkillPointCost, playerLevel.GetSkillPoints());
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, message);
			return;
		}

		if (playerLevel.GetCurrentEnergy() < config.m_iEnergyCost)
		{
			string message = string.Format("Requieres %1 EN (Tienes: %2 EN)", config.m_iEnergyCost, playerLevel.GetCurrentEnergy());
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, message);
			return;
		}
		
		// Check cash requirement
		if (config.m_iCashCost > 0)
		{
			int playerCash = EL_MoneyUtils.GetCash(owner);
			if (playerCash < config.m_iCashCost)
			{
				int cashK = config.m_iCashCost / 1000;
				int haveK = playerCash / 1000;
				string message = string.Format("Requieres $%1K en efectivo (Tienes: $%2K)", cashK, haveK);
				Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, message);
				return;
			}
		}
		
		// Try to unlock the license
		if (licenseManager.UnlockLicense(licenseType, false))
		{
			// Deduct cash after successful unlock
			if (config.m_iCashCost > 0)
			{
				EL_MoneyUtils.RemoveCash(owner, config.m_iCashCost);
			}
			
			int cashK = config.m_iCashCost / 1000;
			string message;
			if (config.m_iCashCost > 0)
				message = string.Format("Licencia adquirida: %1\nCosto: %2 SP + %3 EN + $%4K", config.m_sLicenseName, config.m_iSkillPointCost, config.m_iEnergyCost, cashK);
			else
				message = string.Format("Licencia adquirida: %1\nCosto: %2 SP + %3 EN", config.m_sLicenseName, config.m_iSkillPointCost, config.m_iEnergyCost);
			//Print(string.Format("[EL_UIRPCComponent] SERVER: License %1 purchased successfully", typename.EnumToString(EL_ELicenseType, licenseType)), LogLevel.NORMAL);
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, true, message);
		}
		else
		{
			Rpc(RPC_PurchaseLicenseResult_Owner, licenseType, false, "No se pudo comprar la licencia");
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Receive license purchase result
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_PurchaseLicenseResult_Owner(EL_ELicenseType licenseType, bool success, string message)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT received purchase result: %1, Success: %2", typename.EnumToString(EL_ELicenseType, licenseType), success), LogLevel.NORMAL);
		
		// Show hint to player
		if (success)
		{
			SCR_HintManagerComponent.ShowCustomHint(message, "Â¡ESTUDIOS APRENDIDOS!", 5.0);
			SCR_UISoundEntity.SoundEvent("SOUND_HUD_TASK_SUCCEEDED");
			
			// Request updated university data to refresh the menu
			RPC_RequestUniversityData();
		}
		else
		{
			SCR_HintManagerComponent.ShowCustomHint(message, "ERROR", 3.0);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Banking RPCs
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVER: Open banking menu on client with transaction history and terminal capabilities
	void RPC_OpenBankingMenu(RplId terminalRplId, bool canDeposit, bool canWithdraw, bool canTransfer, array<int> txTypes, array<int> txAmounts, array<float> txTimestamps)
	{
		if (!Replication.IsServer())
			return;
		//Print("[EL_UIRPCComponent] SERVER sending RPC to open banking menu", LogLevel.NORMAL);
		Rpc(RPC_OpenBankingMenu_Owner, terminalRplId, canDeposit, canWithdraw, canTransfer, txTypes, txAmounts, txTimestamps);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenBankingMenu_Owner(RplId terminalRplId, bool canDeposit, bool canWithdraw, bool canTransfer, array<int> txTypes, array<int> txAmounts, array<float> txTimestamps)
	{
		//Print("[EL_UIRPCComponent] CLIENT opening banking menu", LogLevel.NORMAL);

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		EL_BankingMenu menu = EL_BankingMenu.Cast(menuManager.OpenMenu(ChimeraMenuPreset.EL_BankingMenu));
		if (!menu)
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open banking menu on client!", LogLevel.ERROR);
			return;
		}

		// Set player entity from our local player controller
		PlayerController localPC = GetGame().GetPlayerController();
		if (localPC)
		{
			IEntity playerEntity = localPC.GetControlledEntity();
			if (playerEntity)
				menu.SetPlayerEntity(playerEntity);
		}

		// Pass transaction history and terminal flags
		menu.SetBankingDataFromServer(canDeposit, canWithdraw, canTransfer, txTypes, txAmounts, txTimestamps);

		//Print("[EL_UIRPCComponent] Banking menu opened on client", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request bank deposit â€” server validates and executes
	void RPC_BankDeposit(int amount)
	{
		if (Replication.IsServer())
			return;
		//Print(string.Format("[EL_UIRPCComponent] CLIENT requesting deposit: $%1", amount), LogLevel.NORMAL);
		Rpc(RPC_BankDeposit_Server, amount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_BankDeposit_Server(int amount)
	{
		//Print(string.Format("[EL_UIRPCComponent] SERVER processing deposit: $%1", amount), LogLevel.NORMAL);

		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		IEntity player = pc.GetControlledEntity();
		if (!player || amount <= 0)
			return;
		EL_BankAccountComponent bankAccount = EL_Component<EL_BankAccountComponent>.Find(player);
		if (!bankAccount)
		{
			SendHint(WidgetManager.Translate("#DH-Bank_AccountNotFound"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
			return;
		}

		int playerCash = EL_MoneyUtils.GetCash(player);
		if (playerCash < amount)
		{
			SendHint(
				string.Format(WidgetManager.Translate("#DH-Bank_InsCashFmt"), playerCash), WidgetManager.Translate("#DH-Bank_DepositFailTitle"), 3.0
			);
			return;
		}

		int cashTaken = EL_MoneyUtils.TakeCash(player, amount);
		if (cashTaken == amount && bankAccount.Deposit(amount, "DepÃ³sito ATM"))
		{
			SendHint(
				string.Format(WidgetManager.Translate("#DH-Bank_DepositOkFmt"), amount, bankAccount.GetBalance()), WidgetManager.Translate("#DH-Bank_DepositOkTitle"), 3.0
			);
			Rpc(RPC_BankUpdateBalance_Owner, bankAccount.GetBalance(), EL_MoneyUtils.GetCash(player));
		}
		else
		{
			if (cashTaken > 0)
				EL_MoneyUtils.GiveCash(player, cashTaken);
			SendHint(WidgetManager.Translate("#DH-Bank_DepositError"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request bank withdrawal â€” server validates and executes
	void RPC_BankWithdraw(int amount)
	{
		if (Replication.IsServer())
			return;
		//Print(string.Format("[EL_UIRPCComponent] CLIENT requesting withdrawal: $%1", amount), LogLevel.NORMAL);
		Rpc(RPC_BankWithdraw_Server, amount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_BankWithdraw_Server(int amount)
	{
		//Print(string.Format("[EL_UIRPCComponent] SERVER processing withdrawal: $%1", amount), LogLevel.NORMAL);

		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		IEntity player = pc.GetControlledEntity();
		if (!player || amount <= 0)
			return;

		EL_BankAccountComponent bankAccount = EL_Component<EL_BankAccountComponent>.Find(player);
		if (!bankAccount)
		{
			SendHint(WidgetManager.Translate("#DH-Bank_AccountNotFound"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
			return;
		}

		if (bankAccount.GetBalance() < amount)
		{
			SendHint(
				string.Format(WidgetManager.Translate("#DH-Bank_InsBalanceFmt"), bankAccount.GetBalance()), WidgetManager.Translate("#DH-Bank_WithdrawFailTitle"), 3.0
			);
			return;
		}

		if (bankAccount.Withdraw(amount, "Retiro ATM"))
		{
			EL_MoneyUtils.GiveCash(player, amount);
			SendHint(
				string.Format(WidgetManager.Translate("#DH-Bank_WithdrawOkFmt"), amount, bankAccount.GetBalance()), WidgetManager.Translate("#DH-Bank_WithdrawOkTitle"), 3.0
			);
			Rpc(RPC_BankUpdateBalance_Owner, bankAccount.GetBalance(), EL_MoneyUtils.GetCash(player));
		}
		else
		{
			SendHint(WidgetManager.Translate("#DH-Bank_WithdrawError"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request bank transfer to another player â€” server validates and executes
	void RPC_BankTransfer(int amount, RplId targetPlayerRplId)
	{
		if (Replication.IsServer())
			return;
		//Print(string.Format("[EL_UIRPCComponent] CLIENT requesting transfer: $%1", amount), LogLevel.NORMAL);
		Rpc(RPC_BankTransfer_Server, amount, targetPlayerRplId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_BankTransfer_Server(int amount, RplId targetPlayerRplId)
	{
		//Print(string.Format("[EL_UIRPCComponent] SERVER processing transfer: $%1", amount), LogLevel.NORMAL);

		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		IEntity player = pc.GetControlledEntity();
		if (!player || amount <= 0)
			return;

		EL_BankAccountComponent senderAccount = EL_Component<EL_BankAccountComponent>.Find(player);
		if (!senderAccount)
		{
			SendHint(WidgetManager.Translate("#DH-Bank_AccountNotFound"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
			return;
		}

		if (senderAccount.GetBalance() < amount)
		{
			SendHint(
				string.Format(WidgetManager.Translate("#DH-Bank_InsBalanceFmt"), senderAccount.GetBalance()), WidgetManager.Translate("#DH-Bank_TransferFailTitle"), 3.0
			);
			return;
		}

		RplComponent targetRpl = RplComponent.Cast(Replication.FindItem(targetPlayerRplId));
		if (!targetRpl)
		{
			SendHint(WidgetManager.Translate("#DH-Bank_RecipientNotFound"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
			return;
		}

		IEntity targetPlayer = targetRpl.GetEntity();
		if (!targetPlayer)
		{
			SendHint(WidgetManager.Translate("#DH-Bank_RecipientUnavail"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
			return;
		}

		// Bloquear transferencias entre personajes del mismo jugador (misma cuenta de plataforma)
		int senderPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		int targetPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(targetPlayer);
		if (senderPlayerId > 0 && targetPlayerId > 0)
		{
			string senderUID = EL_Utils.GetPlayerSteamUID(senderPlayerId);
			string targetUID = EL_Utils.GetPlayerSteamUID(targetPlayerId);
			if (!senderUID.IsEmpty() && senderUID == targetUID)
			{
				SendHint(WidgetManager.Translate("#DH-Bank_NoSelfTransfer"), WidgetManager.Translate("#DH-Bank_TransferDeniedTitle"), 3.0);
				return;
			}
		}

		EL_BankAccountComponent targetAccount = EL_Component<EL_BankAccountComponent>.Find(targetPlayer);
		if (!targetAccount)
		{
			SendHint(WidgetManager.Translate("#DH-Bank_RecipientNoAccount"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
			return;
		}

		SCR_ChimeraCharacter targetChar = SCR_ChimeraCharacter.Cast(targetPlayer);
		string targetName = "Jugador";
		if (targetChar)
			targetName = EL_Utils.GetCharacterName(targetChar);

		if (senderAccount.Transfer(amount, targetAccount, string.Format("Transferencia a %1", targetName)))
		{
			SendHint(
				string.Format(WidgetManager.Translate("#DH-Bank_TransferOkFmt"), amount, targetName), WidgetManager.Translate("#DH-Bank_TransferOkTitle"), 3.0
			);
			Rpc(RPC_BankUpdateBalance_Owner, senderAccount.GetBalance(), EL_MoneyUtils.GetCash(player));
		}
		else
		{
			SendHint(WidgetManager.Translate("#DH-Bank_TransferError"), WidgetManager.Translate("#DH-Error_Title"), 3.0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SERVERâ†’CLIENT: Update banking menu balance displays after a transaction completes
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_BankUpdateBalance_Owner(int bankBalance, int cashOnHand)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT banking balance update: banco=$%1, efectivo=$%2", bankBalance, cashOnHand), LogLevel.NORMAL);

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		EL_BankingMenu bankMenu = EL_BankingMenu.Cast(menuManager.GetTopMenu());
		if (bankMenu)
			bankMenu.UpdateBalancesFromRPC(bankBalance, cashOnHand);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: Open group bank menu on client
	void RPC_OpenGroupBankMenu(int hideoutId, bool canDeposit, bool canWithdraw)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RPC_OpenGroupBankMenu_Owner, hideoutId, canDeposit, canWithdraw);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenGroupBankMenu_Owner(int hideoutId, bool canDeposit, bool canWithdraw)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		EL_GroupBankMenu menu = EL_GroupBankMenu.Cast(menuManager.OpenMenu(ChimeraMenuPreset.EL_GroupBankMenu));
		if (!menu)
			return;

		PlayerController localPC = GetGame().GetPlayerController();
		if (localPC)
		{
			IEntity playerEntity = localPC.GetControlledEntity();
			if (playerEntity)
				menu.SetPlayerEntity(playerEntity);
		}

		menu.SetGroupBankDataFromServer(hideoutId, canDeposit, canWithdraw);
	}

	//------------------------------------------------------------------------------------------------
	void RPC_GroupBankDeposit(int amount, int hideoutId)
	{
		if (Replication.IsServer())
			return;

		Rpc(RPC_GroupBankDeposit_Server, amount, hideoutId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_GroupBankDeposit_Server(int amount, int hideoutId)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		IEntity player = pc.GetControlledEntity();
		if (!player || amount <= 0)
			return;

		EL_PlayerGroupComponent groupComp;
		EL_GangHideoutComponent hideout;
		if (!ValidateGroupBankAccess(player, hideoutId, groupComp, hideout))
			return;

		int playerCash = EL_MoneyUtils.GetCash(player);
		if (playerCash < amount)
		{
			SendHint(string.Format(WidgetManager.Translate("#DH-Bank_InsCashFmt"), playerCash), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return;
		}

		if (!groupComp.DepositCashToGroup(amount))
		{
			SendHint(WidgetManager.Translate("#DH-GangBank_DepositError"), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return;
		}

		SendHint(string.Format(WidgetManager.Translate("#DH-GangBank_DepositOkFmt"), amount, groupComp.GetGroupBank()), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
		Rpc(RPC_GroupBankUpdateBalance_Owner, groupComp.GetGroupBank(), EL_MoneyUtils.GetCash(player));
	}

	//------------------------------------------------------------------------------------------------
	void RPC_GroupBankWithdraw(int amount, int hideoutId)
	{
		if (Replication.IsServer())
			return;

		Rpc(RPC_GroupBankWithdraw_Server, amount, hideoutId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_GroupBankWithdraw_Server(int amount, int hideoutId)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		IEntity player = pc.GetControlledEntity();
		if (!player || amount <= 0)
			return;

		EL_PlayerGroupComponent groupComp;
		EL_GangHideoutComponent hideout;
		if (!ValidateGroupBankAccess(player, hideoutId, groupComp, hideout))
			return;

		if (!groupComp.IsLeader())
		{
			SendHint(WidgetManager.Translate("#DH-GangBank_LeaderOnly"), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return;
		}

		if (groupComp.GetGroupBank() < amount)
		{
			SendHint(string.Format(WidgetManager.Translate("#DH-GangBank_InsFundsFmt"), groupComp.GetGroupBank()), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return;
		}

		if (!groupComp.WithdrawCashFromGroup(amount))
		{
			SendHint(WidgetManager.Translate("#DH-GangBank_WithdrawError"), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return;
		}

		SendHint(string.Format(WidgetManager.Translate("#DH-GangBank_WithdrawOkFmt"), amount, groupComp.GetGroupBank()), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
		Rpc(RPC_GroupBankUpdateBalance_Owner, groupComp.GetGroupBank(), EL_MoneyUtils.GetCash(player));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_GroupBankUpdateBalance_Owner(int groupBankBalance, int cashOnHand)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		EL_GroupBankMenu bankMenu = EL_GroupBankMenu.Cast(menuManager.GetTopMenu());
		if (bankMenu)
			bankMenu.UpdateBalancesFromRPC(groupBankBalance, cashOnHand);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ValidateGroupBankAccess(IEntity player, int hideoutId, out EL_PlayerGroupComponent groupComp, out EL_GangHideoutComponent hideout)
	{
		groupComp = null;
		hideout = null;

		groupComp = EL_Component<EL_PlayerGroupComponent>.Find(player);
		if (!groupComp || !groupComp.IsInGroup())
		{
			SendHint(WidgetManager.Translate("#DH-GangBank_NeedGang"), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return false;
		}

		EL_GangHideoutManager hideoutMgr = EL_GangHideoutManager.GetInstance();
		if (!hideoutMgr)
		{
			SendHint(WidgetManager.Translate("#DH-GangBank_HideoutUnavail"), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return false;
		}

		hideout = hideoutMgr.GetHideoutById(hideoutId);
		if (!hideout)
		{
			SendHint(WidgetManager.Translate("#DH-GangBank_HideoutNotFound"), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return false;
		}

		if (hideout.GetOwnerGroupId() != groupComp.GetGroupId())
		{
			SendHint(WidgetManager.Translate("#DH-GangBank_NotYourHideout"), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return false;
		}

		if (!hideout.IsPositionInZone(player.GetOrigin()))
		{
			SendHint(WidgetManager.Translate("#DH-GangBank_StayInHideout"), WidgetManager.Translate("#DH-GangBank_Title"), 3.0);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Transport Menu RPCs
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVERâ†’CLIENT: Open transport menu with zone and vehicle data
	void RPC_OpenTransportMenu(RplId zoneRplId, RplId vehicleRplId, int zoneType, string zoneName, float taxRate, float bonusMult, array<int> packedStocks)
	{
		if (!Replication.IsServer())
			return;
		//Print("[EL_UIRPCComponent] SERVER sending RPC to open transport menu", LogLevel.NORMAL);
		Rpc(RPC_OpenTransportMenu_Owner, zoneRplId, vehicleRplId, zoneType, zoneName, taxRate, bonusMult, packedStocks);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenTransportMenu_Owner(RplId zoneRplId, RplId vehicleRplId, int zoneType, string zoneName, float taxRate, float bonusMult, array<int> packedStocks)
	{
		//Print("[EL_UIRPCComponent] CLIENT opening transport menu", LogLevel.NORMAL);

		// Find zone entity from RplId (entity exists on client with its components)
		vector zonePos = vector.Zero;
		IEntity zoneEntity = null;
		RplComponent zoneRplComp = RplComponent.Cast(Replication.FindItem(zoneRplId));
		if (zoneRplComp)
		{
			zoneEntity = zoneRplComp.GetEntity();
			if (zoneEntity)
				zonePos = zoneEntity.GetOrigin();
		}

		// Unpack interleaved stock array [type0, current0, max0, type1, ...] and hydrate local warehouse
		if (zoneEntity && packedStocks && packedStocks.Count() >= 3)
		{
			EL_WarehouseComponent warehouse = EL_WarehouseComponent.Cast(zoneEntity.FindComponent(EL_WarehouseComponent));
			if (warehouse)
			{
				for (int i = 0; i + 2 < packedStocks.Count(); i += 3)
				{
					EL_WarehouseStock stock = warehouse.GetStock(packedStocks[i]);
					if (stock)
					{
						stock.m_iCurrentStock = packedStocks[i + 1];
						stock.m_iMaxStock = packedStocks[i + 2];
					}
				}
			}
		}

		// Find vehicle entity from RplId (may be invalid/null)
		IEntity vehicleEntity = null;
		if (vehicleRplId.IsValid())
		{
			RplComponent vehicleRplComp = RplComponent.Cast(Replication.FindItem(vehicleRplId));
			if (vehicleRplComp)
				vehicleEntity = vehicleRplComp.GetEntity();
		}

		// Get local player entity
		PlayerController localPC = GetGame().GetPlayerController();
		IEntity playerEntity = null;
		if (localPC)
			playerEntity = localPC.GetControlledEntity();

		// Open the menu
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		EL_TransportMenuUI transportMenu = EL_TransportMenuUI.Cast(menuManager.OpenMenu(ChimeraMenuPreset.EL_TransportMenu));
		if (!transportMenu)
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open EL_TransportMenu on client!", LogLevel.ERROR);
			return;
		}

		transportMenu.SetTransportDataFromRPC(playerEntity, vehicleEntity, zoneEntity, zoneType, zoneName, taxRate, bonusMult, zonePos);
		m_ActiveTransportMenu = transportMenu;
		//Print("[EL_UIRPCComponent] Transport menu opened on client", LogLevel.NORMAL);
	}

	// =========================================
	// Transport Load / Unload — Server-side operations
	// =========================================
	
	// Reference to the currently open transport menu (client-side only)
	protected EL_TransportMenuUI m_ActiveTransportMenu;
	
	//------------------------------------------------------------------------------------------------
	//! Helper: get entity from RplId
	protected IEntity EL_GetEntityFromRplId(RplId id)
	{
		if (!id.IsValid()) return null;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if (!rpl) return null;
		return rpl.GetEntity();
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Request loading cargo into a vehicle from a load zone warehouse
	void RPC_TransportLoadCargo(RplId zoneRplId, RplId vehicleRplId, int resourceType, int quantity)
	{
		if (!Replication.IsServer())
		{
			Rpc(RPC_TransportLoadCargo_Server, zoneRplId, vehicleRplId, resourceType, quantity);
			return;
		}
		ProcessTransportLoad(zoneRplId, vehicleRplId, resourceType, quantity);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_TransportLoadCargo_Server(RplId zoneRplId, RplId vehicleRplId, int resourceType, int quantity)
	{
		ProcessTransportLoad(zoneRplId, vehicleRplId, resourceType, quantity);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server: Validate and perform the cargo load operation
	protected void ProcessTransportLoad(RplId zoneRplId, RplId vehicleRplId, int resourceType, int quantity)
	{
		if (!Replication.IsServer()) return;
		if (quantity <= 0) { Rpc(RPC_TransportLoadResult_Owner, false, resourceType, 0); return; }
		
		IEntity zoneEntity = EL_GetEntityFromRplId(zoneRplId);
		if (!zoneEntity) { Rpc(RPC_TransportLoadResult_Owner, false, resourceType, 0); return; }
		
		EL_WarehouseComponent warehouse = EL_WarehouseComponent.Cast(zoneEntity.FindComponent(EL_WarehouseComponent));
		if (!warehouse) { Rpc(RPC_TransportLoadResult_Owner, false, resourceType, 0); return; }
		
		IEntity vehicleEntity = EL_GetEntityFromRplId(vehicleRplId);
		if (!vehicleEntity) { Rpc(RPC_TransportLoadResult_Owner, false, resourceType, 0); return; }
		
		EL_TransportCargoManager cargoManager = EL_TransportCargoManager.Cast(vehicleEntity.FindComponent(EL_TransportCargoManager));
		if (!cargoManager) { Rpc(RPC_TransportLoadResult_Owner, false, resourceType, 0); return; }
		
		EL_EResourceType rt = resourceType;
		
		// Clamp to available vehicle space
		int available = cargoManager.GetAvailableSpace();
		int toLoad = Math.Min(quantity, available);
		if (toLoad <= 0) { Rpc(RPC_TransportLoadResult_Owner, false, resourceType, 0); return; }
		
		// Validate and remove warehouse stock
		EL_WarehouseStock stock = warehouse.GetStock(rt);
		if (!stock || stock.m_iCurrentStock < toLoad)
		{
			if (stock)
				toLoad = stock.m_iCurrentStock;
			else
				toLoad = 0;
			if (toLoad <= 0) { Rpc(RPC_TransportLoadResult_Owner, false, resourceType, 0); return; }
		}
		
		if (!warehouse.RemoveStock(rt, toLoad)) { Rpc(RPC_TransportLoadResult_Owner, false, resourceType, 0); return; }
		
		// Perform load on server-authoritative cargo manager
		EL_EWarehouseType warehouseType = warehouse.GetWarehouseType();
		int loaded = cargoManager.LoadCargo(rt, toLoad, warehouseType);
		
		Print(string.Format("[EL_UIRPCComponent] Server loaded %1 units of type %2 onto vehicle", loaded, resourceType), LogLevel.NORMAL);
		Rpc(RPC_TransportLoadResult_Owner, loaded > 0, resourceType, loaded);
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER→CLIENT: Notify client of load result
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_TransportLoadResult_Owner(bool success, int resourceType, int loadedQty)
	{
		if (!m_ActiveTransportMenu) return;
		m_ActiveTransportMenu.OnTransportLoadResult(success, resourceType, loadedQty);
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Request unloading cargo from vehicle + payment
	void RPC_TransportUnloadAndPay(RplId vehicleRplId, RplId zoneRplId, int playerId, int resourceType, int quantity)
	{
		if (!Replication.IsServer())
		{
			Rpc(RPC_TransportUnloadAndPay_Server, vehicleRplId, zoneRplId, playerId, resourceType, quantity);
			return;
		}
		ProcessTransportUnload(vehicleRplId, zoneRplId, playerId, resourceType, quantity);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_TransportUnloadAndPay_Server(RplId vehicleRplId, RplId zoneRplId, int playerId, int resourceType, int quantity)
	{
		ProcessTransportUnload(vehicleRplId, zoneRplId, playerId, resourceType, quantity);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server: Validate cargo, unload it, calculate and give payment
	protected void ProcessTransportUnload(RplId vehicleRplId, RplId zoneRplId, int playerId, int resourceType, int quantity)
	{
		if (!Replication.IsServer()) return;
		if (quantity <= 0) { Rpc(RPC_TransportUnloadResult_Owner, false, resourceType, 0, 0, 0); return; }
		
		IEntity vehicleEntity = EL_GetEntityFromRplId(vehicleRplId);
		if (!vehicleEntity) { Rpc(RPC_TransportUnloadResult_Owner, false, resourceType, 0, 0, 0); return; }
		
		EL_TransportCargoManager cargoManager = EL_TransportCargoManager.Cast(vehicleEntity.FindComponent(EL_TransportCargoManager));
		if (!cargoManager) { Rpc(RPC_TransportUnloadResult_Owner, false, resourceType, 0, 0, 0); return; }
		
		EL_EResourceType rt = resourceType;
		
		// Validate cargo on server
		int currentCargo = cargoManager.GetCargoAmount(rt);
		if (currentCargo < quantity) quantity = currentCargo;
		if (quantity <= 0) { Rpc(RPC_TransportUnloadResult_Owner, false, resourceType, 0, 0, 0); return; }
		
		// Get zone for payment calculation
		IEntity zoneEntity = EL_GetEntityFromRplId(zoneRplId);
		EL_TransportZoneComponent zone = null;
		EL_WarehouseComponent unloadWarehouse = null;
		if (zoneEntity)
		{
			zone = EL_TransportZoneComponent.Cast(zoneEntity.FindComponent(EL_TransportZoneComponent));
			unloadWarehouse = EL_WarehouseComponent.Cast(zoneEntity.FindComponent(EL_WarehouseComponent));
		}
		
		// Calculate base price per resource type
		int basePrice = 80; // default
		EL_EWarehouseType originWarehouse = cargoManager.GetOriginWarehouse();
		IEntity loadZoneEntity = null;
		// Try to find the load zone's warehouse for the base price
		// As a fallback use default prices per resource type
		switch (rt)
		{
			case EL_EResourceType.WOOD_PLANK:    basePrice = 40; break;
			case EL_EResourceType.FOOD_CRATE:    basePrice = 130; break;
			case EL_EResourceType.PURIFIED_WATER: basePrice = 25; break;
			case EL_EResourceType.FUEL:           basePrice = 80; break;
		}
		
		// Calculate distance bonus
		vector zoneOrigin = vector.Zero;
		if (zoneEntity)
			zoneOrigin = zoneEntity.GetOrigin();
		float distanceBonus = cargoManager.CalculateDistanceBonus(zoneOrigin);
		
		// Calculate payment server-side
		int netPayment;
		int taxPaid;
		if (zone)
		{
			// Use zone's proper calculation
			float routeBonus = zone.GetRouteBonusMultiplier(originWarehouse);
			float taxRate = zone.GetTaxRate();
			float gross = basePrice * quantity * routeBonus * distanceBonus;
			taxPaid = Math.Round(gross * taxRate);
			netPayment = Math.Round(gross) - taxPaid;
		}
		else
		{
			// Fallback: 6.67% tax, 1.25 route bonus
			float gross = basePrice * quantity * 1.25 * distanceBonus;
			taxPaid = Math.Round(gross * 0.0667);
			netPayment = Math.Round(gross) - taxPaid;
		}
		
		// Add level bonus
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (playerEntity)
		{
			EL_PlayerLevelComponent levelComp = EL_PlayerLevelComponent.Cast(playerEntity.FindComponent(EL_PlayerLevelComponent));
			if (levelComp)
			{
				int level = levelComp.GetLevel();
				if (level > 1)
				{
					float levelBonus = 1.0 + ((level - 1) * 0.05);
					netPayment = Math.Round(netPayment * levelBonus);
				}
			}
		}
		
		// Unload cargo
		cargoManager.UnloadCargo(rt, quantity);
		
		// Add to unload zone's warehouse if it has one
		if (unloadWarehouse)
			unloadWarehouse.AddStock(rt, quantity);
		
		// Give cash to player
		if (netPayment > 0 && playerEntity)
		{
			EL_MoneyUtils.AddCash(playerEntity, netPayment);
			
			// Auction system benefits
			EL_AuctionManagerComponent auctionManager = EL_AuctionManagerComponent.GetInstance();
			if (auctionManager)
				auctionManager.AddBenefits(EL_EAuctionSector.TRANSPORTES, netPayment);
		}
		
		// XP: 0.15-0.30 per unit based on distance bonus
		float xpPerUnit = 0.15;
		float totalXP = xpPerUnit * quantity * (1.0 + (distanceBonus - 1.0));
		if (totalXP > 0 && playerEntity)
		{
			EL_PlayerLevelComponent levelComp2 = EL_PlayerLevelComponent.Cast(playerEntity.FindComponent(EL_PlayerLevelComponent));
			if (levelComp2)
				levelComp2.AddExperience(totalXP, "Entrega de transporte");
		}
		
		// Mission system
		if (playerEntity)
		{
			EL_PlayerMissionComponent missionComp = EL_PlayerMissionComponent.Cast(playerEntity.FindComponent(EL_PlayerMissionComponent));
			if (missionComp)
			{
				missionComp.OnTransportCompleted();
				if (netPayment > 0)
					missionComp.OnMoneyEarned(netPayment);
			}
		}
		
		Print(string.Format("[EL_UIRPCComponent] Transport unload: player %1 sold %2x type %3 for $%4 (tax $%5)", 
			playerId, quantity, resourceType, netPayment, taxPaid), LogLevel.NORMAL);
		
		Rpc(RPC_TransportUnloadResult_Owner, true, resourceType, quantity, netPayment, taxPaid);
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER→CLIENT: Notify client of unload result
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_TransportUnloadResult_Owner(bool success, int resourceType, int unloadedQty, int cashGiven, int taxPaid)
	{
		if (!m_ActiveTransportMenu) return;
		m_ActiveTransportMenu.OnTransportUnloadResult(success, resourceType, unloadedQty, cashGiven, taxPaid);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		//Print(string.Format("[EL_UIRPCComponent] Initialized on entity %1", owner), LogLevel.NORMAL);
		
		// CLIENT-SIDE FALLBACK: Si el RPC inicial nunca llega (por timing de replicacion),
		// solicitar reenvio al servidor despues de 4 segundos
		// Aplica solo al PlayerController local (no al de otros jugadores replicados)
		if (!Replication.IsServer())
		{
			PlayerController localPc = GetGame().GetPlayerController();
			PlayerController thisPc = PlayerController.Cast(owner);
			if (thisPc && localPc && thisPc == localPc)
			{
				GetGame().GetCallqueue().CallLater(CheckInitialMenuFallback, INITIAL_MENU_FALLBACK_DELAY_MS, false);
			}
		}
	}
	
	//! CLIENT: Verificar si recibi el menu inicial; si no, solicitar reenvio al servidor
	protected void CheckInitialMenuFallback()
	{
		if (m_bInitialMenuReceived) return;
		
		Print("[EL_UIRPCComponent] WARN: No se recibio ningun menu inicial en 4s. Solicitando reenvio al servidor...", LogLevel.WARNING);
		Rpc(RPC_RequestInitialMenu_Server);
	}
	
	//! SERVER: El cliente no recibio el menu inicial - reenviarlo
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestInitialMenu_Server()
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc) return;
		int playerId = pc.GetPlayerId();
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);
		Print(string.Format("[EL_UIRPCComponent] SERVER: Cliente solicito reenvio de menu para jugador %1 - reenviando WelcomeMenu...", playerId), LogLevel.WARNING);
		RPC_OpenWelcomeMenu(playerId, playerUID);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: Open property menu on client with the property entity reference
	void RPC_OpenPropertyMenu(RplId propertyRplId)
	{
		if (!Replication.IsServer())
			return;
		//Print("[EL_UIRPCComponent] SERVER sending RPC to open property menu", LogLevel.NORMAL);
		
		// Compute access level server-side (EPF UIDs work here, not on client)
		int accessLevel = EL_EPropertyAccessLevel.PUBLIC;
		bool canAccess = false;
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (pc)
		{
			IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(pc.GetPlayerId());
			RplComponent propRpl = RplComponent.Cast(Replication.FindItem(propertyRplId));
			if (propRpl)
			{
				EL_Property property = EL_Property.Cast(propRpl.GetEntity());
				if (property && playerEntity)
				{
					accessLevel = property.GetAccessLevel(playerEntity);
					canAccess = property.CanAccess(playerEntity);
					Print(string.Format("[EL_UIRPCComponent] SERVER computed access=%1, canAccess=%2 for property menu",
						typename.EnumToString(EL_EPropertyAccessLevel, accessLevel), canAccess), LogLevel.NORMAL);
				}
			}
		}
		
		Rpc(RPC_OpenPropertyMenu_Owner, propertyRplId, accessLevel, canAccess);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Open property menu and resolve the property entity from RplId
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenPropertyMenu_Owner(RplId propertyRplId, int accessLevel, bool canAccess)
	{
		Print(string.Format("[EL_UIRPCComponent] CLIENT opening property menu (access=%1, canAccess=%2)",
			typename.EnumToString(EL_EPropertyAccessLevel, accessLevel), canAccess), LogLevel.NORMAL);

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc) return;
		IEntity playerEntity = pc.GetControlledEntity();
		if (!playerEntity) return;

		EL_PropertyMenu menu = EL_PropertyMenu.Cast(menuManager.OpenMenu(ChimeraMenuPreset.EL_PropertyMenu));
		if (!menu)
			return;

		// Set server-computed access BEFORE SetProperty (which calls UpdateButtonStates)
		menu.SetPrecomputedAccess(accessLevel, canAccess);

		// Resolve property entity from RplId
		RplComponent propRpl = RplComponent.Cast(Replication.FindItem(propertyRplId));
		if (propRpl)
		{
			EL_Property property = EL_Property.Cast(propRpl.GetEntity());
			if (property)
				menu.SetProperty(property);
		}

		menu.SetPlayerEntity(playerEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request server to sell a property
	void RPC_RequestPropertySell(RplId propertyRplId)
	{
		if (Replication.IsServer())
			return;
		Rpc(RPC_PropertySell_Server, propertyRplId);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: Process property sell request
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_PropertySell_Server(RplId propertyRplId)
	{
		RplComponent propRpl = RplComponent.Cast(Replication.FindItem(propertyRplId));
		if (!propRpl) return;
		EL_Property property = EL_Property.Cast(propRpl.GetEntity());
		if (!property) return;
		property.Sell();
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request server to purchase a property
	void RPC_RequestPropertyPurchase(RplId propertyRplId)
	{
		if (Replication.IsServer())
			return;
		Rpc(RPC_PropertyPurchase_Server, propertyRplId);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: Process property purchase request
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_PropertyPurchase_Server(RplId propertyRplId)
	{
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController) return;
		
		int playerId = playerController.GetPlayerId();
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!playerEntity) return;
		
		RplComponent propRpl = RplComponent.Cast(Replication.FindItem(propertyRplId));
		if (!propRpl) return;
		EL_Property property = EL_Property.Cast(propRpl.GetEntity());
		if (!property) return;
		
		bool success = property.PerformPurchaseOnServer(playerEntity);
		
		// Reopen menu on client after replication delay.
		// 1200ms on success gives time for RplProp (m_sOwnerUID, m_bOwned) to reach the client
		// before the menu reopens and reads the access level.
		int delayMs = 200;
		if (success)
		{
			delayMs = 1200;
			// Enviar marcador personal de la propiedad al jugador comprador
			vector propPos = property.GetOrigin();
			string propName = "Mi propiedad";
			EL_PropertyConfig propCfg = property.GetConfig();
			if (propCfg && !propCfg.m_sPropertyName.IsEmpty())
				propName = propCfg.m_sPropertyName;
			Rpc(RPC_ShowPersonalMapMarker_Owner, propPos[0], propPos[2], propName);
		}
		GetGame().GetCallqueue().CallLater(DelayedReopenPropertyMenuAfterPurchase, delayMs, false, propertyRplId);
	}

	//------------------------------------------------------------------------------------------------
	protected void DelayedReopenPropertyMenuAfterPurchase(RplId propertyRplId)
	{
		RPC_OpenPropertyMenu(propertyRplId);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request server to toggle lock on a property
	void RPC_RequestPropertyToggleLock(RplId propertyRplId)
	{
		if (Replication.IsServer())
			return;
		Rpc(RPC_PropertyToggleLock_Server, propertyRplId);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: Process property toggle lock request
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_PropertyToggleLock_Server(RplId propertyRplId)
	{
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController) return;

		int playerId = playerController.GetPlayerId();
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!playerEntity) return;

		RplComponent propRpl = RplComponent.Cast(Replication.FindItem(propertyRplId));
		if (!propRpl) return;
		EL_Property property = EL_Property.Cast(propRpl.GetEntity());
		if (!property) return;

		//Print(string.Format("[EL_UIRPCComponent] SERVER ToggleLock request for propertyRplId=%1", propertyRplId), LogLevel.NORMAL);
		property.PerformToggleLockOnServer(playerEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: Request the server to cleanup the dead character so the player can respawn
	//! This RPC goes through the PlayerController (which survives character death)
	//! instead of the character entity (which is dead and can't send RPCs)
	void RequestRespawnCleanup()
	{
		//Print("[EL_UIRPCComponent] CLIENT requesting respawn cleanup via PlayerController", LogLevel.NORMAL);
		Rpc(RPC_RequestRespawnCleanup_Server);
	}
	
	//------------------------------------------------------------------------------------------------
	//! SERVER: Cleanup the dead character (kill, restore health, save, delete)
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestRespawnCleanup_Server()
	{
		//Print("[EL_UIRPCComponent] RPC_RequestRespawnCleanup_Server received", LogLevel.NORMAL);
		
		PlayerController playerController = PlayerController.Cast(GetOwner());
		if (!playerController)
		{
			Print("[EL_UIRPCComponent] ERROR: Owner is not a PlayerController!", LogLevel.ERROR);
			return;
		}
		
		int playerId = playerController.GetPlayerId();
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		
		if (!controlledEntity)
		{
			Print(string.Format("[EL_UIRPCComponent] RespawnCleanup: No controlled entity for player %1 - may already be cleaned up", playerId), LogLevel.WARNING);
			return;
		}
		
		//Print(string.Format("[EL_UIRPCComponent] RespawnCleanup: Processing dead character for player %1", playerId), LogLevel.NORMAL);
		
		// 0. Capture corpse data BEFORE any entity changes (for cadaver spawn later)
		vector deathPos = controlledEntity.GetOrigin();
		vector deathRot = controlledEntity.GetYawPitchRoll();
		array<ResourceName> equipPrefabs = new array<ResourceName>();
		EquipedLoadoutStorageComponent deadLoadout = EquipedLoadoutStorageComponent.Cast(
			controlledEntity.FindComponent(EquipedLoadoutStorageComponent));
		if (deadLoadout)
		{
			array<IEntity> equippedItems = {};
			deadLoadout.GetAll(equippedItems);
			foreach (IEntity eqItem : equippedItems)
			{
				if (eqItem)
				{
					EntityPrefabData prefabData = eqItem.GetPrefabData();
					if (prefabData)
						equipPrefabs.Insert(prefabData.GetPrefabName());
				}
			}
		}
		
		// 1. Kill the incapacitated character to trigger proper death
		EL_IncapacitationComponent incapComp = EL_IncapacitationComponent.Cast(controlledEntity.FindComponent(EL_IncapacitationComponent));
		if (incapComp && incapComp.IsIncapacitated())
		{
			incapComp.KillForRespawn();
			//Print("[EL_UIRPCComponent] RespawnCleanup: KillForRespawn called", LogLevel.NORMAL);
		}
		
		// 2. Restore health before saving (so character spawns alive next time)
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(controlledEntity.FindComponent(SCR_DamageManagerComponent));
		if (damageManager)
		{
			array<HitZone> hitZones = {};
			damageManager.GetAllHitZones(hitZones);
			foreach (HitZone hitZone : hitZones)
			{
				if (hitZone)
					hitZone.SetHealth(hitZone.GetMaxHealth());
			}
			damageManager.SetHealthScaled(1.0);
			//Print(string.Format("[EL_UIRPCComponent] RespawnCleanup: Restored %1 hitzones to full health", hitZones.Count()), LogLevel.NORMAL);
		}
		
		// 3. Restore survival stats
		EL_SurvivalStatsComponent survivalComp = EL_SurvivalStatsComponent.Cast(controlledEntity.FindComponent(EL_SurvivalStatsComponent));
		if (survivalComp)
		{
			survivalComp.SetFood(75);
			survivalComp.SetWater(75);
			survivalComp.SetStamina(100);
		}
		
		// 4. Save character data and delete entity
		EPF_PersistenceComponent characterPersistence = EPF_Component<EPF_PersistenceComponent>.Find(controlledEntity);
		if (characterPersistence)
		{
			//Print("[EL_UIRPCComponent] RespawnCleanup: Saving character before deletion", LogLevel.NORMAL);
			characterPersistence.PauseTracking();
			characterPersistence.SetPreventDbDeletion(true);
			characterPersistence.Save();
			
			// Delete after save completes
			GetGame().GetCallqueue().CallLater(DeleteCharacterEntity, 1000, false, controlledEntity);
		}
		else
		{
			SCR_EntityHelper.DeleteEntityAndChildren(controlledEntity);
		}
		
		// 4b. Read apartment spawn upgrade BEFORE entity is deleted (DeleteCharacterEntity fires 1s later)
		bool hasApartSpawn = false;
		float aptSpawnX = 0, aptSpawnY = 0, aptSpawnZ = 0;
		{
			EL_ApartmentPlayerComponent apartComp = EL_ApartmentPlayerComponent.Cast(
				controlledEntity.FindComponent(EL_ApartmentPlayerComponent));
			if (apartComp && apartComp.HasApartment() && apartComp.HasSpawnUpgrade())
			{
				hasApartSpawn = true;
				vector spawnPos = EL_ApartmentManagerComponent.GetInteriorSpawnWorldPos(apartComp.GetApartmentPosition());
				aptSpawnX = spawnPos[0];
				aptSpawnY = spawnPos[1];
				aptSpawnZ = spawnPos[2];
			}
		}

		// 5. Clear death state in account and send character data back to client
		string playerUID = EL_Utils.GetPlayerSteamUID(playerId);
		string charId = "";
		string charPrefab = "";
		string charFirstName = "";
		string charLastName = "";
		int charAge = 25;
		
		if (!playerUID.IsEmpty())
		{
			EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerUID);
			if (account)
			{
				EL_PlayerCharacter character = account.GetActiveCharacter();
				if (character)
				{
					// Extract character data BEFORE clearing death state
					charId = character.GetId();
					charPrefab = character.GetPrefab();
					charFirstName = character.GetFirstName();
					charLastName = character.GetLastName();
					charAge = character.GetAge();
					
					character.SetWasDeadOnDisconnect(false);
					character.SetDeathTimestamp(0);
					character.SetDeathMenuTimeRemaining(0);
					account.SafeSave();
					//Print(string.Format("[EL_UIRPCComponent] RespawnCleanup: Death state cleared. CharID=%1, Prefab=%2", charId, charPrefab), LogLevel.NORMAL);
				}
			}
		}
		
		// 6. Spawn cadaver character clone at death location (with player's equipment)
		EL_CorpseManagerComponent corpseManager = EL_CorpseManagerComponent.GetInstance();
		if (corpseManager && !charPrefab.IsEmpty())
		{
			string deceasedName = charFirstName;
			if (!charLastName.IsEmpty())
				deceasedName = deceasedName + " " + charLastName;
			corpseManager.SpawnCadaver(deathPos, deathRot, deceasedName, charPrefab, equipPrefabs);
		}
		
		// 7. Send character data back to client so it can open spawn menu
		if (!charId.IsEmpty())
		{
			//Print(string.Format("[EL_UIRPCComponent] RespawnCleanup: Sending character data to client: %1 %2 (ID: %3)", charFirstName, charLastName, charId), LogLevel.NORMAL);
			string aptSpawnPos = string.Format("%1 %2 %3", aptSpawnX, aptSpawnY, aptSpawnZ);
			Rpc(RPC_RespawnDataReady_Owner, playerId, playerUID, charId, charFirstName, charLastName, charAge, charPrefab, aptSpawnPos);
		}
		else
		{
			Print("[EL_UIRPCComponent] RespawnCleanup: WARNING - No character data found, client will use fallback", LogLevel.WARNING);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT: Receive character data from server after respawn cleanup
	//! This allows the client to open the spawn menu with proper character data
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_RespawnDataReady_Owner(int playerId, string playerUID, string characterId, string firstName, string lastName, int age, string prefab, string aptSpawnPos)
	{
		//Print(string.Format("[EL_UIRPCComponent] CLIENT received respawn data: %1 %2 (ID: %3, Prefab: %4)", firstName, lastName, characterId, prefab), LogLevel.NORMAL);

		// Parsear posición de spawn del apartamento (empaquetada como "x y z")
		float aptSpawnX = 0, aptSpawnY = 0, aptSpawnZ = 0;
		if (!aptSpawnPos.IsEmpty())
		{
			array<string> aptParts = {};
			aptSpawnPos.Split(" ", aptParts, true);
			if (aptParts.Count() >= 3)
			{
				aptSpawnX = aptParts[0].ToFloat();
				aptSpawnY = aptParts[1].ToFloat();
				aptSpawnZ = aptParts[2].ToFloat();
			}
		}

		// aptSpawnX != 0 se usa como centinela: 0 = sin upgrade, != 0 = tiene upgrade
		if (aptSpawnX != 0)
		{
			EL_SpawnPointManager.InitializeDefaultSpawnPoints();

			// Evitar duplicado si el jugador muere varias veces en la misma sesión
			bool alreadyAdded = false;
			array<ref EL_SpawnPointConfig> existing = EL_SpawnPointManager.GetAllSpawnPoints();
			if (existing)
			{
				foreach (EL_SpawnPointConfig sp : existing)
				{
					if (sp && sp.GetDisplayName() == "Apartamento")
					{
						alreadyAdded = true;
						break;
					}
				}
			}

			if (!alreadyAdded)
			{
				EL_SpawnPointConfig aptConfig = new EL_SpawnPointConfig();
				aptConfig.m_sDisplayName = "Apartamento";
				aptConfig.m_sDescription = "Tu apartamento privado";
				aptConfig.m_vPosition = Vector(aptSpawnX, aptSpawnY, aptSpawnZ);
				aptConfig.m_vRotation = Vector(270, 0, 0);
				aptConfig.m_vPreviewCameraPosition = Vector(aptSpawnX, aptSpawnY + 6, aptSpawnZ - 10);
				aptConfig.m_vPreviewCameraTarget   = Vector(aptSpawnX, aptSpawnY + 1, aptSpawnZ);
				EL_SpawnPointManager.RegisterSpawnPoint(aptConfig);
			}
		}

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			Print("[EL_UIRPCComponent] ERROR: MenuManager is null on client!", LogLevel.ERROR);
			return;
		}
		
		// Open spawn selection menu with the data from the server
		MenuBase menu = menuManager.OpenMenu(ChimeraMenuPreset.SpawnSelectionMenu);
		EL_SpawnSelectionMenu spawnMenu = EL_SpawnSelectionMenu.Cast(menu);
		if (spawnMenu)
		{
			if (!prefab.IsEmpty())
			{
				spawnMenu.SetPlayerData(playerId, playerUID, characterId, firstName, lastName, age, prefab);
				//Print(string.Format("[EL_UIRPCComponent] Spawn menu opened with server data for character %1", characterId), LogLevel.NORMAL);
			}
			else
			{
				spawnMenu.SetPlayerDataForRespawn(playerId, characterId);
				//Print(string.Format("[EL_UIRPCComponent] Spawn menu opened with limited data for character %1", characterId), LogLevel.NORMAL);
			}
		}
		else
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open SpawnSelectionMenu!", LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enviar posiciones de jugadores visibles al cliente para el mapa GPS
	//! Llamado desde EL_MapTrackerComponent en el servidor
	void RPC_UpdateMapMarkers(notnull array<float> posX, notnull array<float> posZ, notnull array<int> types, notnull array<string> names)
	{
		Rpc(RPC_UpdateMapMarkers_Owner, posX, posZ, types, names);
	}

	//------------------------------------------------------------------------------------------------
	//! RPC recibido en el CLIENTE con las posiciones de jugadores visibles
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_UpdateMapMarkers_Owner(notnull array<float> posX, notnull array<float> posZ, notnull array<int> types, notnull array<string> names)
	{
		// Actualiza GPS minimap + mapa principal (M)
		EL_ClientMapMarkerManager.UpdateMarkers(posX, posZ, types, names);
	}

	// =====================================================================
	//  MARCADORES DE EVENTO DE TRABAJO (robos, llamadas de emergencia)
	// =====================================================================

	//! SERVERâ†’CLIENT: Enviar marcador de evento a este jugador especÃ­fico.
	//! Llamar solo desde el servidor iterando los PlayerControllers del trabajo.
	//! ttlMs: tiempo en ms antes del borrado automÃ¡tico (por defecto 15 min = 900000)
	void SendJobEventMarker(float posX, float posZ, string text, int ttlMs = 900000, int iconType = 0)
	{
		Rpc(RPC_ShowJobEventMarker_Owner, posX, posZ, text, ttlMs, iconType);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ShowJobEventMarker_Owner(float posX, float posZ, string text, int ttlMs, int iconType)
	{
		SCR_EScenarioFrameworkMarkerCustom icon;
		SCR_EScenarioFrameworkMarkerCustomColor color;
		string customImageset;   // si no está vacío, sobreescribe el icono con este imageset
		string customQuad;
		switch (iconType)
		{
			case 1: // EMS – icono "help" verde
				icon  = SCR_EScenarioFrameworkMarkerCustom.POINT_OF_INTEREST;
				color = SCR_EScenarioFrameworkMarkerCustomColor.GREEN;
				customImageset = "{67B3A6DC2D712B52}UI/Textures/Icons/icons_mapMarkersUI-glow.imageset";
				customQuad = "help";
				break;
			case 2: // Tirador activo – icono "destroy-2" rojo
				icon  = SCR_EScenarioFrameworkMarkerCustom.OBJECTIVE_MARKER;
				color = SCR_EScenarioFrameworkMarkerCustomColor.RED;
				customImageset = "{67B3A6DC2D712B52}UI/Textures/Icons/icons_mapMarkersUI-glow.imageset";
				customQuad = "destroy-2";
				break;
			case 3: // Legacy – amarillo (search area)
				icon  = SCR_EScenarioFrameworkMarkerCustom.SEARCH_AREA;
				color = SCR_EScenarioFrameworkMarkerCustomColor.YELLOW;
				break;
			case 4: // Furgón blindado – icono "defend" rojo – zona RDM
				icon  = SCR_EScenarioFrameworkMarkerCustom.OBJECTIVE_MARKER;
				color = SCR_EScenarioFrameworkMarkerCustomColor.RED;
				customImageset = "{67B3A6DC2D712B52}UI/Textures/Icons/icons_mapMarkersUI-glow.imageset";
				customQuad = "defend";
				break;
			case 5: // Vuelco al Narco – icono "destroy-2" rojo – zona RDM
				icon  = SCR_EScenarioFrameworkMarkerCustom.OBJECTIVE_MARKER;
				color = SCR_EScenarioFrameworkMarkerCustomColor.RED;
				customImageset = "{67B3A6DC2D712B52}UI/Textures/Icons/icons_mapMarkersUI-glow.imageset";
				customQuad = "destroy-2";
				break;
			default: // 0 = genérico rojo
				icon  = SCR_EScenarioFrameworkMarkerCustom.OBJECTIVE_MARKER;
				color = SCR_EScenarioFrameworkMarkerCustomColor.RED;
				break;
		}
		SCR_MapMarkerBase marker = EL_ClientMapMarkerManager.AddEventMarker(posX, posZ, text, icon, color);

		// Sobreescribir el icono con uno custom del imageset mapMarkersUI-glow si aplica
		if (marker && !customImageset.IsEmpty())
		{
			SCR_MapMarkerWidgetComponent widgetComp = marker.GetMarkerComponent();
			if (widgetComp)
				widgetComp.SetImage(customImageset, customQuad);
		}

		// Auto-borrar tras ttlMs milisegundos (0 = sin TTL, el borrado es manual)
		if (ttlMs > 0)
			GetGame().GetCallqueue().CallLater(EL_ClientMapMarkerManager.ClearAllEventMarkers, ttlMs, false);
	}

	//! SERVERâ†’CLIENT: Borrar todos los marcadores de evento de este jugador.
	void SendClearJobEventMarkers()
	{
		Rpc(RPC_ClearJobEventMarkers_Owner);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ClearJobEventMarkers_Owner()
	{
		EL_ClientMapMarkerManager.ClearAllEventMarkers();
	}

	// =====================================================================
	//  MARCADORES PERSONALES (propiedades compradas)
	// =====================================================================

	//! SERVERâ†’CLIENT: AÃ±adir marcador personal (solo visible para este jugador).
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ShowPersonalMapMarker_Owner(float posX, float posZ, string text)
	{
		EL_ClientMapMarkerManager.AddPersonalMarker(posX, posZ, text);
	}

	// =====================================================================
	//  LLAMADA DE EMERGENCIA POR SMS (cliente pide al servidor difundir posiciÃ³n)
	// =====================================================================

	//! CLIENT: Solicitar al servidor que envÃ­e la posiciÃ³n del llamante a policÃ­a o EMS.
	//! jobTypeInt = EL_EJobType (POLICE = 100, MEDIC = 4)
	//! ttlMs: tiempo en ms antes del borrado del marcador (por defecto 900000 = 15 min)
	void RPC_RequestEmergencyCallMarker(int jobTypeInt, int ttlMs = 900000)
	{
		if (Replication.IsServer())
			return;
		Rpc(RPC_RequestEmergencyCallMarker_Server, jobTypeInt, ttlMs);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestEmergencyCallMarker_Server(int jobTypeInt, int ttlMs)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		int callerPlayerId = pc.GetPlayerId();
		IEntity callerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(callerPlayerId);
		if (!callerEntity)
			return;

		vector callerPos = callerEntity.GetOrigin();
		float posX = callerPos[0];
		float posZ = callerPos[2];

		EL_EJobType jobType = EL_EJobType.POLICE;
		if (jobTypeInt == EL_EJobType.MEDIC)
			jobType = EL_EJobType.MEDIC;

		string callerName = EL_Utils.GetCharacterName(SCR_ChimeraCharacter.Cast(callerEntity));
		string markerText;
		if (jobType == EL_EJobType.POLICE)
			markerText = string.Format("LLAMADA POLICIA - %1", callerName);
		else
			markerText = string.Format("LLAMADA EMS - %1", callerName);

		// Enviar marcador a todos los jugadores activos del trabajo indicado
		PlayerManager pm = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		pm.GetPlayers(playerIds);
		foreach (int pid : playerIds)
		{
			IEntity pEntity = pm.GetPlayerControlledEntity(pid);
			if (!pEntity)
				continue;

			EL_PlayerJobComponent jobComp = EL_Component<EL_PlayerJobComponent>.Find(pEntity);
			if (!jobComp || jobComp.GetJob() != jobType)
				continue;

			PlayerController targetPc = pm.GetPlayerController(pid);
			if (!targetPc)
				continue;

			EL_UIRPCComponent targetRpc = EL_UIRPCComponent.Cast(targetPc.FindComponent(EL_UIRPCComponent));
			if (!targetRpc)
				continue;

			targetRpc.SendJobEventMarker(posX, posZ, markerText, ttlMs, 1); // iconType 1 = EMS/plus (verde)
		}

		Print(string.Format("[EL_UIRPCComponent] Emergency call marker sent to %1 players with job %2 (TTL %3ms)",
			playerIds.Count(), jobType, ttlMs), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Helper: Delete character entity after save (called via CallLater)
	protected static void DeleteCharacterEntity(IEntity entity)
	{
		if (entity)
		{
			//Print("[EL_UIRPCComponent] Deleting character entity after save", LogLevel.NORMAL);
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}

	// =====================================================================
	//  REVIVE VIA RESPAWN
	// =====================================================================

	//------------------------------------------------------------------------------------------------
	//! SERVER: Notify client to close death menu for a respawn-based revive.
	//! Called from EL_DeathManagerComponent.ReviveViaRespawn().
	//! Uses PlayerController as RPC channel â€” works even when character entity is "dead".
	void NotifyReviveRespawn()
	{
		if (!Replication.IsServer())
			return;

		Rpc(RPC_NotifyReviveRespawn_Owner);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_NotifyReviveRespawn_Owner()
	{
		// Close the death menu â€” the server is handling the full respawn cycle
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
		{
			MenuBase topMenu = menuManager.GetTopMenu();
			EL_DeathMenuUI deathMenu;
			if (topMenu)
				deathMenu = EL_DeathMenuUI.Cast(topMenu);

			if (deathMenu)
				deathMenu.Close();
			else
				menuManager.CloseMenuByPreset(ChimeraMenuPreset.DeathMenu);
		}

		SCR_HintManagerComponent.ShowCustomHint(WidgetManager.Translate("#DH-Revive_BeingRevived"), WidgetManager.Translate("#DH-Revive_Title"), 3.0);

		//Print("[EL_UIRPCComponent] RPC_NotifyReviveRespawn_Owner: Death menu closed for respawn-based revive", LogLevel.NORMAL);
	}

	// =====================================================================
	//  ADMIN REVIVE
	// =====================================================================

	//------------------------------------------------------------------------------------------------
	//! SERVER: Llamado desde EL_AdminRevivePlayerContextAction para notificar al cliente revivido.
	//! Usa el PlayerController como canal RPC â€” funciona aunque la entidad del personaje estÃ© "muerta".
	void AdminRevivePlayer()
	{
		if (!Replication.IsServer())
			return;

		Rpc(RPC_AdminRevivePlayer_Owner);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_AdminRevivePlayer_Owner()
	{
		// 1. Cerrar el menÃº de muerte usando Close() directo (mÃ¡s fiable que CloseMenuByPreset)
		//    El flujo de respawn usa Close() y funciona siempre; CloseMenuByPreset puede fallar
		//    silenciosamente si el menÃº modal no se encuentra correctamente en el stack.
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
		{
			MenuBase topMenu = menuManager.GetTopMenu();
			EL_DeathMenuUI deathMenu;
			if (topMenu)
				deathMenu = EL_DeathMenuUI.Cast(topMenu);

			if (deathMenu)
			{
				deathMenu.Close();
				//Print("[EL_UIRPCComponent] RPC_AdminRevivePlayer_Owner: Death menu closed via Close()", LogLevel.NORMAL);
			}
			else
			{
				// Fallback: intentar con CloseMenuByPreset por si hay otro menÃº encima
				menuManager.CloseMenuByPreset(ChimeraMenuPreset.DeathMenu);
				Print("[EL_UIRPCComponent] RPC_AdminRevivePlayer_Owner: Top menu is not DeathMenu, used CloseMenuByPreset as fallback", LogLevel.WARNING);
			}
		}

		// 2. Resetear estado en los componentes del personaje
		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		if (character)
		{
			EL_DeathManagerComponent deathManager = EL_DeathManagerComponent.Cast(character.FindComponent(EL_DeathManagerComponent));
			if (deathManager)
				deathManager.OnAdminRevive();

			EL_IncapacitationComponent incap = EL_IncapacitationComponent.Cast(character.FindComponent(EL_IncapacitationComponent));
			if (incap)
				incap.ResetIncapacitatedFlag();

			// 3. Restaurar controles con breve delay (el cierre de menÃº procesa de forma asÃ­ncrona)
			GetGame().GetCallqueue().CallLater(RestoreCharacterControlsAfterAdminRevive, 100, false);
		}

		// 4. Mostrar hint (funciona directamente en cliente, sin RPC adicional)
		SCR_HintManagerComponent.ShowCustomHint(WidgetManager.Translate("#DH-Admin_Revived"), WidgetManager.Translate("#DH-Admin_RevivedTitle"), 5.0);

		//Print("[EL_UIRPCComponent] RPC_AdminRevivePlayer_Owner: Admin revive applied on client", LogLevel.NORMAL);

		// 5. VerificaciÃ³n de seguridad: tras 500ms re-verificar si queda algÃºn menÃº de muerte abierto
		GetGame().GetCallqueue().CallLater(VerifyDeathMenuClosed, 500, false);
	}

	//------------------------------------------------------------------------------------------------
	//! VerificaciÃ³n de seguridad post-revive: si el menÃº de muerte sigue abierto, forzar cierre.
	protected void VerifyDeathMenuClosed()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MenuBase topMenu = menuManager.GetTopMenu();
		if (!topMenu)
			return;

		EL_DeathMenuUI deathMenu = EL_DeathMenuUI.Cast(topMenu);
		if (deathMenu)
		{
			Print("[EL_UIRPCComponent] VerifyDeathMenuClosed: Death menu still open! Force closing.", LogLevel.WARNING);
			deathMenu.Close();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RestoreCharacterControlsAfterAdminRevive()
	{
		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		if (!character)
			return;

		SCR_CharacterControllerComponent charController = SCR_CharacterControllerComponent.Cast(
			character.FindComponent(SCR_CharacterControllerComponent));
		if (charController)
		{
			charController.SetDisableMovementControls(false);
			charController.SetDisableWeaponControls(false);
		}

		//Print("[EL_UIRPCComponent] RestoreCharacterControlsAfterAdminRevive: controls restored", LogLevel.NORMAL);
	}

	// =====================================================================
	//  EXAMEN DE CONDUCIR â€” INDICADOR 3D DE CHECKPOINT
	// =====================================================================

	//! SERVERâ†’CLIENT: Mostrar indicador 3D de checkpoint al jugador propietario.
	//! Llamado por EL_DrivingExamManagerComponent al enviar cada checkpoint.
	void SendDrivingExamCheckpoint(float worldX, float worldY, float worldZ, string text)
	{
		Rpc(RPC_ShowDrivingCheckpoint_Owner, worldX, worldY, worldZ, text);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ShowDrivingCheckpoint_Owner(float worldX, float worldY, float worldZ, string text)
	{
		EL_DrivingExamHUD.ShowCheckpoint(worldX, worldY, worldZ, text);
	}

	//! SERVERâ†’CLIENT: Ocultar indicador 3D de checkpoint del jugador propietario.
	//! Llamado al finalizar un checkpoint o al terminar el examen.
	void SendDrivingExamHide()
	{
		Rpc(RPC_HideDrivingCheckpoint_Owner);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_HideDrivingCheckpoint_Owner()
	{
		EL_DrivingExamHUD.Hide();
	}
	//------------------------------------------------------------------------------------------------
	//! SERVER: Mostrar barra de progreso de captura
	void ShowCaptureProgress(string title, int duration)
	{
		if (!Replication.IsServer()) return;
		Rpc(RPC_ShowCaptureProgress_Owner, title, duration);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ShowCaptureProgress_Owner(string title, int duration)
	{
		EL_CaptureProgressUI.ShowProgressBar(GetOwner(), title, duration);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: Actualizar barra de progreso de captura
	void UpdateCaptureProgress(float progress)
	{
		if (!Replication.IsServer()) return;
		Rpc(RPC_UpdateCaptureProgress_Owner, progress);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_UpdateCaptureProgress_Owner(float progress)
	{
		if (!EL_CaptureProgressUI.IsProgressBarShown())
			EL_CaptureProgressUI.ShowProgressBar(GetOwner(), "Escondite", 0);
			
		EL_CaptureProgressUI.UpdateProgress(progress);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: Ocultar barra de progreso de captura
	void HideCaptureProgress()
	{
		if (!Replication.IsServer()) return;
		Rpc(RPC_HideCaptureProgress_Owner);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_HideCaptureProgress_Owner()
	{
		EL_CaptureProgressUI.HideProgressBar();
	}

	// =====================================================================
	//  EMPRESA - MISIONES DE TRÁFICO
	// =====================================================================

	//! SERVER→CLIENT: Abrir el menú de empresa con el pool de misiones.
	void RPC_OpenEmpresaMenu(notnull array<int> types, notnull array<string> names, notnull array<string> descs, notnull array<int> rewards, notnull array<int> xpRewards, notnull array<string> destNames)
	{
		// En Workbench (host-local) no hay replicación real → ejecutar localmente
		if (!Replication.IsRunning())
		{
			RPC_OpenEmpresaMenu_Owner(types, names, descs, rewards, xpRewards, destNames);
			return;
		}
		if (!Replication.IsServer())
			return;
		Rpc(RPC_OpenEmpresaMenu_Owner, types, names, descs, rewards, xpRewards, destNames);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenEmpresaMenu_Owner(notnull array<int> types, notnull array<string> names, notnull array<string> descs, notnull array<int> rewards, notnull array<int> xpRewards, notnull array<string> destNames)
	{
		MenuManager menuMgr = GetGame().GetMenuManager();
		if (!menuMgr)
			return;

		MenuBase menuBase = menuMgr.OpenMenu(ChimeraMenuPreset.EL_EmpresaMenu);
		if (!menuBase)
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open EL_EmpresaMenu", LogLevel.ERROR);
			return;
		}

		EL_EmpresaMenuUI menu = EL_EmpresaMenuUI.Cast(menuBase);
		if (menu)
			menu.SetMissionData(types, names, descs, rewards, xpRewards, destNames);
	}

	//! CLIENT→SERVER: Solicitar inicio de misión en el slot indicado.
	void RPC_RequestStartEmpresaMission(int slot)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestStartEmpresaMission_Server(slot);
			return;
		}
		Rpc(RPC_RequestStartEmpresaMission_Server, slot);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestStartEmpresaMission_Server(int slot)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		int playerId = pc.GetPlayerId();
		EL_EmpresaManagerComponent mgr = EL_EmpresaManagerComponent.GetInstance();
		if (mgr)
			mgr.StartMission(playerId, slot);
	}

	//! SERVER→CLIENT: Marcador personal del punto de entrega (solo para el jugador que hace la misión).
	void RPC_ShowDeliveryMarker(float posX, float posZ, string text)
	{
		// En Workbench (host-local) no hay replicación real → ejecutar localmente
		if (!Replication.IsRunning())
		{
			RPC_ShowDeliveryMarker_Owner(posX, posZ, text);
			return;
		}
		if (!Replication.IsServer())
			return;
		Rpc(RPC_ShowDeliveryMarker_Owner, posX, posZ, text);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ShowDeliveryMarker_Owner(float posX, float posZ, string text)
	{
		if (text.IsEmpty() || text == "null")
			text = "Punto de entrega";

		// Usar marcador de evento en vez de personal para evitar problemas de widget de persistencia.
		SCR_MapMarkerBase marker = EL_ClientMapMarkerManager.AddEventMarker(posX, posZ, text, SCR_EScenarioFrameworkMarkerCustom.OBJECTIVE_MARKER, SCR_EScenarioFrameworkMarkerCustomColor.YELLOW);
		if (marker)
			GetGame().GetCallqueue().CallLater(EL_ClientMapMarkerManager.ClearAllEventMarkers, 600000, false);
	}

	// =====================================================================
	//  MAFIA STOCK - SUMINISTROS DE ARMAS
	// =====================================================================

	//! SERVER→CLIENT: Abrir menú de suministros con pool de contratos y datos de stock.
	void RPC_OpenMafiaSupplyMenu(notnull array<int> types, notnull array<string> names, notnull array<string> descs,
		notnull array<int> stockRewards, notnull array<string> destNames, notnull array<int> expirySeconds, string shopName, int currentStock)
	{
		// En Workbench host-local no hay replicación real
		if (Replication.IsRunning() && !Replication.IsServer())
			return;

		if (!Replication.IsRunning())
		{
			RPC_OpenMafiaSupplyMenu_Owner(types, names, descs, stockRewards, destNames, expirySeconds, shopName, currentStock);
			return;
		}

		Rpc(RPC_OpenMafiaSupplyMenu_Owner, types, names, descs, stockRewards, destNames, expirySeconds, shopName, currentStock);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenMafiaSupplyMenu_Owner(notnull array<int> types, notnull array<string> names, notnull array<string> descs,
		notnull array<int> stockRewards, notnull array<string> destNames, notnull array<int> expirySeconds, string shopName, int currentStock)
	{
		MenuManager menuMgr = GetGame().GetMenuManager();
		if (!menuMgr)
			return;

		MenuBase menuBase = menuMgr.OpenMenu(ChimeraMenuPreset.EL_MafiaSupplyMenu);
		if (!menuBase)
		{
			Print("[EL_UIRPCComponent] ERROR: Failed to open EL_MafiaSupplyMenu", LogLevel.ERROR);
			return;
		}

		EL_MafiaSupplyMenuUI menu = EL_MafiaSupplyMenuUI.Cast(menuBase);
		if (menu)
			menu.SetSupplyData(types, names, descs, stockRewards, destNames, expirySeconds, shopName, currentStock);
	}

	//! CLIENT→SERVER: Solicitar inicio de misión de suministro.
	void RPC_RequestStartSupplyMission(int slot, string targetShopName)
	{
		// Workbench host-local: la misma instancia es cliente+servidor
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestStartSupplyMission_Server(slot, targetShopName);
			return;
		}

		Rpc(RPC_RequestStartSupplyMission_Server, slot, targetShopName);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestStartSupplyMission_Server(int slot, string targetShopName)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		int playerId = pc.GetPlayerId();
		EL_MafiaStockManagerComponent mgr = EL_MafiaStockManagerComponent.GetInstance();
		if (mgr)
			mgr.StartSupplyMission(playerId, slot, targetShopName);
	}
<<<<<<< HEAD

	// =====================================================================
	// BUSINESS OWNERSHIP SYSTEM RPCs
	// =====================================================================

	//------------------------------------------------------------------------------------------------
	//! SERVER→CLIENT: Abrir menú de gestión de negocios
	//! Datos empaquetados en strings para respetar el límite de parámetros de Rpc():
	//!   bizInfo  = "businessType|ownerUID|ownerName|purchasePrice|priceModifier|revenue|totalRevenue|playerUID"
	//!   empData  = "uid1,name1,role1;uid2,name2,role2;..."
	//!   stockData = "prefab1,qty1;prefab2,qty2;..."
	void RPC_OpenBusinessMenu(string shopName, string bizInfo, string empData, string stockData)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_OpenBusinessMenu_Owner(shopName, bizInfo, empData, stockData);
			return;
		}

		Rpc(RPC_OpenBusinessMenu_Owner, shopName, bizInfo, empData, stockData);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenBusinessMenu_Owner(string shopName, string bizInfo, string empData, string stockData)
	{
		MenuBase menu = GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.EL_BusinessMenu);
		EL_BusinessMenuUI bizMenu = EL_BusinessMenuUI.Cast(menu);
		if (!bizMenu)
			return;

		bizMenu.SetBusinessDataPacked(shopName, bizInfo, empData, stockData);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Comprar un negocio
	void RPC_RequestBuyBusiness(string shopName, int businessType)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestBuyBusiness_Server(shopName, businessType);
			return;
		}
		Rpc(RPC_RequestBuyBusiness_Server, shopName, businessType);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestBuyBusiness_Server(string shopName, int businessType)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		SCR_PlayerController scrPc = SCR_PlayerController.Cast(pc);
		if (!scrPc)
			return;

		IEntity player = scrPc.GetMainEntity();
		if (!player)
			return;

		EL_BusinessManagerComponent mgr = EL_BusinessManagerComponent.GetInstance();
		if (!mgr)
			return;

		bool success = mgr.PurchaseBusiness(player, shopName, businessType);
		if (success)
		{
			SendHint("#DH-Biz_PurchaseSuccess", "#DH-Biz_Title", 5);

			// Marcador personal del negocio en el mapa (igual que propiedades)
			vector playerPos = player.GetOrigin();
			string markerText = string.Format("Mi negocio: %1", shopName);
			Rpc(RPC_ShowPersonalMapMarker_Owner, playerPos[0], playerPos[2], markerText);
		}
		else
			SendHint("#DH-Biz_PurchaseFailed", "#DH-Biz_Title", 5);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Vender un negocio
	void RPC_RequestSellBusiness(string shopName)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestSellBusiness_Server(shopName);
			return;
		}
		Rpc(RPC_RequestSellBusiness_Server, shopName);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestSellBusiness_Server(string shopName)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		SCR_PlayerController scrPc = SCR_PlayerController.Cast(pc);
		if (!scrPc)
			return;

		IEntity player = scrPc.GetMainEntity();
		if (!player)
			return;

		EL_BusinessManagerComponent mgr = EL_BusinessManagerComponent.GetInstance();
		if (!mgr)
			return;

		bool success = mgr.SellBusiness(player, shopName);
		if (success)
			SendHint("#DH-Biz_SellSuccess", "#DH-Biz_Title", 5);
		else
			SendHint("#DH-Biz_SellFailed", "#DH-Biz_Title", 5);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Cobrar ingresos acumulados
	void RPC_RequestCollectRevenue(string shopName)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestCollectRevenue_Server(shopName);
			return;
		}
		Rpc(RPC_RequestCollectRevenue_Server, shopName);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestCollectRevenue_Server(string shopName)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		SCR_PlayerController scrPc = SCR_PlayerController.Cast(pc);
		if (!scrPc)
			return;

		IEntity player = scrPc.GetMainEntity();
		if (!player)
			return;

		EL_BusinessManagerComponent mgr = EL_BusinessManagerComponent.GetInstance();
		if (!mgr)
			return;

		int collected = mgr.CollectRevenue(player, shopName);
		if (collected > 0)
			SendHint(string.Format("#DH-Biz_RevenueCollected $%1", collected), "#DH-Biz_Title", 5);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Iniciar misión de restock
	void RPC_RequestStartRestockMission(string shopName)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestStartRestockMission_Server(shopName);
			return;
		}
		Rpc(RPC_RequestStartRestockMission_Server, shopName);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestStartRestockMission_Server(string shopName)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		int playerId = pc.GetPlayerId();
		EL_BusinessManagerComponent mgr = EL_BusinessManagerComponent.GetInstance();
		if (!mgr)
			return;

		bool success = mgr.StartRestockMission(playerId, shopName);
		if (success)
			SendHint("#DH-Biz_RestockStarted", "#DH-Biz_Title", 5);
		else
			SendHint("#DH-Biz_RestockFailed", "#DH-Biz_Title", 5);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Ajustar precio del negocio
	void RPC_RequestSetPriceModifier(string shopName, float modifier)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestSetPriceModifier_Server(shopName, modifier);
			return;
		}
		Rpc(RPC_RequestSetPriceModifier_Server, shopName, modifier);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestSetPriceModifier_Server(string shopName, float modifier)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		string playerUID = GetGame().GetBackendApi().GetPlayerIdentityId(pc.GetPlayerId());

		EL_BusinessManagerComponent mgr = EL_BusinessManagerComponent.GetInstance();
		if (!mgr)
			return;

		mgr.SetPriceModifier(shopName, playerUID, modifier);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Contratar empleado
	void RPC_RequestHireEmployee(string shopName, int targetPlayerId, int role)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestHireEmployee_Server(shopName, targetPlayerId, role);
			return;
		}
		Rpc(RPC_RequestHireEmployee_Server, shopName, targetPlayerId, role);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestHireEmployee_Server(string shopName, int targetPlayerId, int role)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		string ownerUID = GetGame().GetBackendApi().GetPlayerIdentityId(pc.GetPlayerId());
		string empUID = GetGame().GetBackendApi().GetPlayerIdentityId(targetPlayerId);
		string empName = GetGame().GetPlayerManager().GetPlayerName(targetPlayerId);

		EL_BusinessManagerComponent mgr = EL_BusinessManagerComponent.GetInstance();
		if (!mgr)
			return;

		bool success = mgr.HireEmployee(shopName, ownerUID, empUID, empName, role);
		if (success)
		{
			SendHint(string.Format("#DH-Biz_Hired %1", empName), "#DH-Biz_Title", 5);

			// Notificar al empleado
			EL_NotificationManagerComponent notifMgr = EL_NotificationManagerComponent.GetInstance();
			if (notifMgr)
			{
				EL_NotificationConfig notif = new EL_NotificationConfig("#DH-Biz_Title", string.Format("#DH-Biz_YouWereHired %1", shopName), 8);
				notifMgr.SendToPlayer(targetPlayerId, notif);
			}
		}
		else
			SendHint("#DH-Biz_HireFailed", "#DH-Biz_Title", 5);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT→SERVER: Despedir empleado
	void RPC_RequestFireEmployee(string shopName, string employeeUID)
	{
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			RPC_RequestFireEmployee_Server(shopName, employeeUID);
			return;
		}
		Rpc(RPC_RequestFireEmployee_Server, shopName, employeeUID);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RPC_RequestFireEmployee_Server(string shopName, string employeeUID)
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		string ownerUID = GetGame().GetBackendApi().GetPlayerIdentityId(pc.GetPlayerId());

		EL_BusinessManagerComponent mgr = EL_BusinessManagerComponent.GetInstance();
		if (!mgr)
			return;

		bool success = mgr.FireEmployee(shopName, ownerUID, employeeUID);
		if (success)
			SendHint("#DH-Biz_Fired", "#DH-Biz_Title", 5);
		else
			SendHint("#DH-Biz_FireFailed", "#DH-Biz_Title", 5);
	}
=======
>>>>>>> parent of 14e5cb9 (Add business system, PublicWorks job & tools)
}
