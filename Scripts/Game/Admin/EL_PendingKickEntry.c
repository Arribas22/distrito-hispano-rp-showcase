//------------------------------------------------------------------------------------------------
//! Entrada de kick pendiente en la base de datos (EDF_DbEntity)
//! El panel crea una entrada, el servidor la lee y expulsa al jugador.
//! El ID del documento es autogenerado (UUID).
[EDF_DbName.Automatic()]
class EL_PendingKickEntry : EDF_DbEntity
{
	string m_sPlayerUID;
	string m_sReason;
	string m_sKickedBy;
	int m_iTimestamp;

	//------------------------------------------------------------------------------------------------
	void EL_PendingKickEntry()
	{
	}

	//------------------------------------------------------------------------------------------------
	static EL_PendingKickEntry Create(string playerUID, string reason, string kickedBy)
	{
		EL_PendingKickEntry entry = new EL_PendingKickEntry();
		entry.m_sPlayerUID = playerUID;
		entry.m_sReason = reason;
		entry.m_sKickedBy = kickedBy;
		entry.m_iTimestamp = System.GetUnixTime();
		return entry;
	}
}
