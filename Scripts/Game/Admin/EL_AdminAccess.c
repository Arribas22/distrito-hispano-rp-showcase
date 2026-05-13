class EL_AdminAccess
{
	//------------------------------------------------------------------------------------------------
	static bool IsPlayerAdmin(IEntity playerEntity = null)
	{
		int playerId = GetPlayerId(playerEntity);
		if (playerId <= 0)
			return false;

		return SCR_Global.IsAdmin(playerId);
	}

	//------------------------------------------------------------------------------------------------
	static int GetPlayerId(IEntity playerEntity = null)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && playerEntity)
		{
			int entityPlayerId = playerManager.GetPlayerIdFromControlledEntity(playerEntity);
			if (entityPlayerId > 0)
				return entityPlayerId;
		}

		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId > 0)
			return localPlayerId;

		PlayerController playerController = GetGame().GetPlayerController();
		if (playerController)
			return playerController.GetPlayerId();

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static void ShowNoAdminAccessHint(string title = "ADMIN")
	{
		SCR_HintManagerComponent.ShowCustomHint(WidgetManager.Translate("#DH-Admin_NoPerms"), title, 3.0);
	}
}