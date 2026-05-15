//------------------------------------------------------------------------------------------------
//! UI RPC Component — Public Showcase Excerpt
//! Componente adjunto al PlayerController para enviar RPCs servidor → cliente
//! que abren menús de UI o envían notificaciones.
//!
//! NOTA: Este archivo es un extracto representativo. La implementación de producción
//! contiene ~30 RPCs adicionales (inventario, vehículos, menús de rol, etc.)
//! no incluidos aquí para proteger la propiedad intelectual del servidor.
//!
//! PATRÓN GENERAL:
//!   1. Método público (llamado desde servidor) → invoca Rpc(RPC_*_Owner, ...)
//!   2. Método [RplRpc(Reliable, Owner)] protegido → ejecutado en el cliente propietario
//!   3. Los datos complejos se serializan en arrays paralelos de primitivos (replicables)
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Core", description: "Handles RPC calls for opening client UI")]
class EL_UIRPCComponentClass : ScriptComponentClass
{
}

class EL_UIRPCComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	// EJEMPLO 1: RPC simple — Enviar un hint al cliente
	//
	// Este es el patrón más básico: el servidor llama al método público con los datos,
	// que internamente dispara el RPC al cliente propietario del PlayerController.
	//------------------------------------------------------------------------------------------------

	//! SERVER: Envía un hint de texto al cliente propietario
	void SendHint(string message, string title, float duration)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RPC_SendHint_Owner, message, title, duration);
	}

	//! CLIENT: Recibe el hint y lo muestra mediante SCR_HintManagerComponent
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_SendHint_Owner(string message, string title, float duration)
	{
		SCR_HintManagerComponent.ShowCustomHint(message, title, duration);
	}


	//------------------------------------------------------------------------------------------------
	// EJEMPLO 2: RPC con reintento — Abrir WelcomeMenu en el cliente
	//
	// El MenuManager puede no estar disponible inmediatamente cuando el RPC llega.
	// Se implementa un mecanismo de reintento con CallLater para evitar
	// que el jugador quede sin menú en conexiones lentas.
	//------------------------------------------------------------------------------------------------

	//! SERVER: Envía RPC para abrir el WelcomeMenu en el cliente
	void RPC_OpenWelcomeMenu(int playerId, string playerUID)
	{
		Rpc(RPC_OpenWelcomeMenu_Owner, playerId, playerUID);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenWelcomeMenu_Owner(int playerId, string playerUID)
	{
		TryOpenWelcomeMenu(playerId, playerUID, 0);
	}

	//! Abre el menú con reintentos si el MenuManager aún no está disponible
	protected void TryOpenWelcomeMenu(int playerId, string playerUID, int retries)
	{
		const int MAX_RETRIES = 3;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			if (retries < MAX_RETRIES)
				GetGame().GetCallqueue().CallLater(TryOpenWelcomeMenu, 500, false, playerId, playerUID, retries + 1);
			else
				Print("[EL_UIRPCComponent] ERROR: MenuManager unavailable after max retries.", LogLevel.ERROR);
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
	}


	//------------------------------------------------------------------------------------------------
	// EJEMPLO 3: RPC con datos complejos — Abrir CharacterSelectionMenu
	//
	// Arma Reforger solo puede replicar tipos primitivos en RPCs.
	// Para enviar objetos complejos (lista de personajes con varios campos),
	// se serializan en arrays paralelos: ids[], firstNames[], lastNames[], ...
	// El cliente reconstruye los objetos localmente a partir de esos arrays.
	//------------------------------------------------------------------------------------------------

	//! SERVER: Serializa los personajes del account en arrays paralelos y envía el RPC
	void RPC_OpenCharacterSelectionMenu(int playerId, string playerUID, EL_PlayerAccount account)
	{
		array<string> ids = {};
		array<string> firstNames = {};
		array<string> lastNames = {};
		array<int> ages = {};
		array<string> prefabs = {};
		array<bool> hasSpawned = {};

		foreach (EL_PlayerCharacter character : account.GetCharacters())
		{
			ids.Insert(character.GetId());
			firstNames.Insert(character.GetFirstName());
			lastNames.Insert(character.GetLastName());
			ages.Insert(character.GetAge());
			prefabs.Insert(character.GetPrefab());
			hasSpawned.Insert(character.HasSpawnedBefore());
		}

		Rpc(RPC_OpenCharacterSelectionMenu_Owner, playerId, playerUID,
			ids, firstNames, lastNames, ages, prefabs, hasSpawned);
	}

	//! CLIENT: Recibe los arrays y abre el menú de selección de personaje
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_OpenCharacterSelectionMenu_Owner(
		int playerId, string playerUID,
		array<string> ids, array<string> firstNames, array<string> lastNames,
		array<int> ages, array<string> prefabs, array<bool> hasSpawned)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			Print("[EL_UIRPCComponent] ERROR: MenuManager not found on client.", LogLevel.ERROR);
			return;
		}

		EL_CharacterSelectionMenu menu = EL_CharacterSelectionMenu.Cast(
			menuManager.OpenMenu(ChimeraMenuPreset.CharacterSelectionMenu)
		);

		if (menu)
			menu.SetPlayerData(playerId, playerUID, ids, firstNames, lastNames, ages, prefabs, hasSpawned);
	}

	// ... Implementación completa omitida (propiedad intelectual privada)
}
