//------------------------------------------------------------------------------------------------
//! Entrada de whitelist de mafia persistente en la base de datos (EDF_DbEntity)
//! El ID del documento es el Steam UID del jugador en la whitelist
[EDF_DbName.Automatic()]
class EL_MafiaWhitelistEntry : EDF_DbEntity
{
	string m_sPlayerName;
	string m_sAddedBy;
	int m_iAddedTimestamp;

	//------------------------------------------------------------------------------------------------
	void EL_MafiaWhitelistEntry()
	{
	}

	//------------------------------------------------------------------------------------------------
	static EL_MafiaWhitelistEntry Create(string playerUID, string playerName, string addedBy)
	{
		EL_MafiaWhitelistEntry entry = new EL_MafiaWhitelistEntry();
		entry.SetId(playerUID);
		entry.m_sPlayerName = playerName;
		entry.m_sAddedBy = addedBy;
		entry.m_iAddedTimestamp = System.GetUnixTime();
		return entry;
	}
}
