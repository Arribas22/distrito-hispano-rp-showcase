//------------------------------------------------------------------------------------------------
//! Entrada de cambio de licencia pendiente en la base de datos (EDF_DbEntity)
//! El panel crea una entrada para grant/revoke, el servidor la lee y aplica en memoria.
//! El ID del documento es autogenerado (UUID).
[EDF_DbName.Automatic()]
class EL_PendingLicenseChangeEntry : EDF_DbEntity
{
	string m_sPlayerUID;
	int m_iLicenseId;
	string m_sAction;       // "grant" o "revoke"
	string m_sChangedBy;
	int m_iTimestamp;

	//------------------------------------------------------------------------------------------------
	void EL_PendingLicenseChangeEntry()
	{
	}

	//------------------------------------------------------------------------------------------------
	static EL_PendingLicenseChangeEntry Create(string playerUID, int licenseId, string action, string changedBy)
	{
		EL_PendingLicenseChangeEntry entry = new EL_PendingLicenseChangeEntry();
		entry.m_sPlayerUID = playerUID;
		entry.m_iLicenseId = licenseId;
		entry.m_sAction = action;
		entry.m_sChangedBy = changedBy;
		entry.m_iTimestamp = System.GetUnixTime();
		return entry;
	}
}
