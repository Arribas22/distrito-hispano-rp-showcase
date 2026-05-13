//------------------------------------------------------------------------------------------------
//! Entrada de whitelist de EMS persistente en la base de datos (EDF_DbEntity)
//! El ID del documento es el Steam UID del jugador en la whitelist
[EDF_DbName.Automatic()]
class EL_EmsWhitelistEntry : EDF_DbEntity
{
	string m_sPlayerName;
	string m_sAddedBy;
	int m_iAddedTimestamp;

	//------------------------------------------------------------------------------------------------
	void EL_EmsWhitelistEntry()
	{
	}

	//------------------------------------------------------------------------------------------------
	static EL_EmsWhitelistEntry Create(string playerUID, string playerName, string addedBy)
	{
		EL_EmsWhitelistEntry entry = new EL_EmsWhitelistEntry();
		entry.SetId(playerUID);
		entry.m_sPlayerName = playerName;
		entry.m_sAddedBy = addedBy;
		entry.m_iAddedTimestamp = System.GetUnixTime();
		return entry;
	}
}
