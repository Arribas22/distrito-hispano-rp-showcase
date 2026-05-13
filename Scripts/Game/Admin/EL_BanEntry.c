//------------------------------------------------------------------------------------------------
//! Entrada de ban persistente en la base de datos (EDF_DbEntity)
//! El ID del documento es el Steam UID del jugador baneado
[EDF_DbName.Automatic()]
class EL_BanEntry : EDF_DbEntity
{
	string m_sPlayerName;
	string m_sBannedBy;
	int m_iBanTimestamp;
	string m_sReason;

	//------------------------------------------------------------------------------------------------
	void EL_BanEntry()
	{
	}

	//------------------------------------------------------------------------------------------------
	static EL_BanEntry Create(string playerUID, string playerName, string bannedBy, string reason = "Ban de administrador")
	{
		EL_BanEntry entry = new EL_BanEntry();
		entry.SetId(playerUID);
		entry.m_sPlayerName = playerName;
		entry.m_sBannedBy = bannedBy;
		entry.m_iBanTimestamp = System.GetUnixTime();
		entry.m_sReason = reason;
		return entry;
	}
}
