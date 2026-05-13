//! Modded override: keeps improved warning messages for broken components
//! but ABORTS the save on ERROR (vanilla behavior) to prevent partial DB writes.
//! A failed save keeps the existing DB data intact — far safer than overwriting
//! with missing components (which permanently loses level/XP/licenses/bank).
modded class EPF_EntitySaveData
{
	override EPF_EReadResult ReadFrom(IEntity entity, EPF_EntitySaveDataClass attributes)
	{
		EPF_EReadResult statusCode = EPF_EReadResult.DEFAULT;
		if (!attributes.m_bTrimDefaults)
			statusCode = EPF_EReadResult.OK;

		EPF_PersistenceComponent persistenceComponent = EPF_Component<EPF_PersistenceComponent>.Find(entity);
		ReadMetaData(persistenceComponent);

		// Prefab
		m_rPrefab = EPF_Utils.GetPrefabName(entity);

		// Transform
		m_pTransformation = new EPF_PersistentTransformation();
		EPF_EPersistenceFlags flags = persistenceComponent.GetFlags();
		if (EPF_BitFlags.CheckFlags(flags, EPF_EPersistenceFlags.ROOT) &&
			(!EPF_BitFlags.CheckFlags(flags, EPF_EPersistenceFlags.BAKED) || EPF_BitFlags.CheckFlags(flags, EPF_EPersistenceFlags.WAS_MOVED)))
		{
			if (m_pTransformation.ReadFrom(entity, attributes, true))
				statusCode = EPF_EReadResult.OK;
		}

		// Lifetime
		if (EPF_BitFlags.CheckFlags(flags, EPF_EPersistenceFlags.ROOT) &&
			attributes.m_bSaveRemainingLifetime)
		{
			auto garbage = SCR_GarbageSystem.GetByEntityWorld(entity);
			if (garbage)
				m_fRemainingLifetime = garbage.GetRemainingLifetime(entity);

			if (m_fRemainingLifetime == -1)
			{
				m_fRemainingLifetime = 0;
			}
			else if (m_fRemainingLifetime > 0)
			{
				statusCode = EPF_EReadResult.OK;
			}
		}

		// Components
		array<Managed> processedComponents();

		foreach (EPF_ComponentSaveDataClass componentSaveDataClass : attributes.m_aComponents)
		{
			typename saveDataType = EPF_Utils.TrimEnd(componentSaveDataClass.ClassName(), 5).ToType();
			if (!saveDataType)
			{
				PrintFormat("[EPF_EntitySaveData] ERROR: Component save-data class '%1' not implemented. Aborting save to protect DB integrity.", componentSaveDataClass.ClassName());
				return EPF_EReadResult.ERROR;
			}

			array<Managed> outComponents();
			entity.FindComponents(EPF_ComponentSaveDataType.Get(componentSaveDataClass.Type()), outComponents);
			foreach (Managed componentRef : outComponents)
			{
				if (processedComponents.Contains(componentRef))
					continue;

				processedComponents.Insert(componentRef);

				EPF_ComponentSaveData componentSaveData = EPF_ComponentSaveData.Cast(saveDataType.Spawn());
				if (!componentSaveData)
				{
					PrintFormat("[EPF_EntitySaveData] ERROR: Failed to instantiate '%1'. Aborting save to protect DB integrity.", saveDataType.ToString());
					return EPF_EReadResult.ERROR;
				}

				componentSaveDataClass.m_bTrimDefaults = attributes.m_bTrimDefaults;
				EPF_EReadResult componentRead = componentSaveData.ReadFrom(entity, GenericComponent.Cast(componentRef), componentSaveDataClass);

				// ABORT on ERROR — the DB keeps its existing (complete) data.
				// A partial save would overwrite the DB missing this component,
				// permanently losing whatever data that component held.
				if (componentRead == EPF_EReadResult.ERROR)
				{
					PrintFormat("[EPF_EntitySaveData] ERROR: Component '%1' (save-data '%2') failed ReadFrom. Aborting entire save to protect DB data.", componentRef.ClassName(), saveDataType.ToString());
					return componentRead;
				}

				if (componentRead == EPF_EReadResult.DEFAULT && attributes.m_bTrimDefaults)
					continue;

				EPF_PersistentComponentSaveData persistentComponent();
				persistentComponent.m_pData = componentSaveData;
				m_aComponents.Insert(persistentComponent);
			}
		}

		if (!m_aComponents.IsEmpty())
			statusCode = EPF_EReadResult.OK;

		return statusCode;
	}
}
