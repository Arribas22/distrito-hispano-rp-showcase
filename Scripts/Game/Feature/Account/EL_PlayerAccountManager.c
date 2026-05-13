class EL_PlayerAccountManager
{
	protected static ref EL_PlayerAccountManager s_pInstance;
	protected ref map<string, ref EL_PlayerAccount> m_mAccounts;

	//------------------------------------------------------------------------------------------------
	//! Async loading of a players account
	//! \param playerUid The players Bohemia UID
	//! \param create If true a new account will be created if none is found for the UID
	//! \param callback Async callback to handle the result
	void LoadAccountAsync(string playerUid, bool create, notnull EDF_DataCallbackSingle<EL_PlayerAccount> callback)
	{
		EL_PlayerAccount account = m_mAccounts.Get(playerUid);
		if (account)
		{
			callback.Invoke(account);
			return;
		}

		auto processorCallback = EL_PlayerAccountManagerProcessorCallback.Create(playerUid, create, callback);
		EPF_PersistentScriptedStateLoader<EL_PlayerAccount>.LoadAsync(playerUid, processorCallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Save and pending changes on the account and release it from the manager. Used primarily on disconnect of player
	//! \param playerUid The players Bohemia UID
	void SaveAndReleaseAccount(notnull EL_PlayerAccount account)
	{
		account.PauseTracking();
		account.SafeSave();
		m_mAccounts.Remove(account.GetPersistentId());
	}

	//------------------------------------------------------------------------------------------------
	//! Add the player accountt instance to the cache so it is returned on the next LoadAccountAsync call
	//! \param account Account instance to cache
	void AddToCache(notnull EL_PlayerAccount account)
	{
		m_mAccounts.Set(account.GetPersistentId(), account);
	}

	//------------------------------------------------------------------------------------------------
	EL_PlayerAccount GetFromCache(string playerUid)
	{
		return m_mAccounts.Get(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Get account from cache using player ID
	EL_PlayerAccount GetFromCache(int playerId)
	{
		return GetFromCache(EL_Utils.GetPlayerSteamUID(playerId)); // Use Steam UID for account lookup
	}

	//------------------------------------------------------------------------------------------------
	//! Get account from cache using player entity
	EL_PlayerAccount GetFromCache(IEntity player)
	{
		return GetFromCache(EL_Utils.GetPlayerSteamUID(player)); // Use Steam UID for account lookup
	}

	//------------------------------------------------------------------------------------------------
	static EL_PlayerAccountManager GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new EL_PlayerAccountManager();

		return s_pInstance;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_pInstance = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void EL_PlayerAccountManager()
	{
		m_mAccounts = new map<string, ref EL_PlayerAccount>();
	}
};

class EL_PlayerAccountManagerProcessorCallback : EDF_DataCallbackSingle<EL_PlayerAccount>
{
	string m_sPlayerUid
	bool m_bCreate;
	ref EDF_DataCallbackSingle<EL_PlayerAccount> m_pCallback;

	//------------------------------------------------------------------------------------------------
	override void OnComplete(EL_PlayerAccount data, Managed context)
	{
		if (data)
		{
			Print(string.Format("[EL_PlayerAccountManager] SUCCESS: Loaded account for UID %1. Character count: %2", m_sPlayerUid, data.GetCharacterCount()), LogLevel.NORMAL);
			if (data.GetCharacterCount() > 0)
			{
				EL_PlayerCharacter active = data.GetActiveCharacter();
				if (active)
					Print(string.Format("[EL_PlayerAccountManager] Active character: %1 (ID: %2)", active.GetFullName(), active.GetId()), LogLevel.NORMAL);
				else
					Print("[EL_PlayerAccountManager] WARNING: Account has characters but no active character returned!", LogLevel.WARNING);
			}
		}
		else
		{
			Print(string.Format("[EL_PlayerAccountManager] FAILED: No account found for UID %1 (Create=%2)", m_sPlayerUid, m_bCreate), LogLevel.WARNING);
		}

		EL_PlayerAccount result = data; //Keep explicit strong ref to it or else create on null will fail
		if (!result && m_bCreate)
		{
			Print(string.Format("[EL_PlayerAccountManager] Creating NEW account for UID %1", m_sPlayerUid), LogLevel.NORMAL);
			result = EL_PlayerAccount.Create(m_sPlayerUid);
		}

		if (result)
			EL_PlayerAccountManager.GetInstance().AddToCache(result);

		if (m_pCallback)
			m_pCallback.Invoke(result);
	}

	//------------------------------------------------------------------------------------------------
	static EL_PlayerAccountManagerProcessorCallback Create(string playerUid, bool create, EDF_DataCallbackSingle<EL_PlayerAccount> callback)
	{
		EL_PlayerAccountManagerProcessorCallback instance();
		instance.m_sPlayerUid = playerUid;
		instance.m_bCreate = create;
		instance.m_pCallback = callback;
		return instance;
	}
};
