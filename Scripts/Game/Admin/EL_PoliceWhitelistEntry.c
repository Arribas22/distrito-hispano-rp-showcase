//------------------------------------------------------------------------------------------------
//! Entrada de whitelist de policía persistente en la base de datos (EDF_DbEntity)
//! El ID del documento es el Steam UID del jugador en la whitelist
[EDF_DbName.Automatic()]
class EL_PoliceWhitelistEntry : EDF_DbEntity
{
	string m_sPlayerName;
	string m_sAddedBy;
	int m_iAddedTimestamp;

	//------------------------------------------------------------------------------------------------
	void EL_PoliceWhitelistEntry()
	{
	}

	//------------------------------------------------------------------------------------------------
	static EL_PoliceWhitelistEntry Create(string playerUID, string playerName, string addedBy)
	{
		EL_PoliceWhitelistEntry entry = new EL_PoliceWhitelistEntry();
		entry.SetId(playerUID);
		entry.m_sPlayerName = playerName;
		entry.m_sAddedBy = addedBy;
		entry.m_iAddedTimestamp = System.GetUnixTime();
		return entry;
	}
}
