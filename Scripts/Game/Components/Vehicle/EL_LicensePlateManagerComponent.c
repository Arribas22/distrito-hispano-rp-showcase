[EntityEditorProps(category: "EL/LicnesePlateManagerComponent", description: "This renders license plates on your vehicle.", color: "0 0 255 255")]
class EL_LicensePlateManagerComponentClass: ScriptComponentClass
{
};

class EL_LicensePlatePointInfo: PointInfo
{	
	EL_LicensePlateEntity m_Object;
	
	void ~EL_LicensePlatePointInfo()
	{
		if (m_Object)
		{
			delete m_Object;
		}
	}
}

class EL_LicensePlateManagerComponent: ScriptComponent
{
	[Attribute(uiwidget: UIWidgets.Auto)]
	ref array<ref EL_LicensePlatePointInfo> m_Plates;
	
	[Attribute("{E95486C43308F36B}Prefabs/Vehicles/LicensePlate/LicensePlate.et")]
	protected ResourceName m_LicensePlatePrefab;
	
	[RplProp(onRplName: "OnRegistrationUpdated")]
	string m_Registration;
	
	//! Plate entities registered by external systems (e.g., slot-spawned ReloadZ plates)
	protected ref array<EL_LicensePlateEntity> m_aRegisteredPlates = {};
	
	override void EOnInit(IEntity owner)
	{
		// Generate registration on server if not already set (e.g., by garage restore)
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (GetGame().InPlayMode() && rpl && rpl.IsMaster() && m_Registration.IsEmpty())
		{
			Resource container = BaseContainerTools.LoadContainer("{B1DD7B5D4812AB19}Configs/Vehicles/VehicleSettings.conf");
			if (container && container.IsValid())
			{
				EL_VehicleSettings vehicleSettings = EL_VehicleSettings.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
				if (vehicleSettings && vehicleSettings.m_LicensePlateGenerator)
				{
					m_Registration = vehicleSettings.m_LicensePlateGenerator.GenerateLicensePlate();
					Replication.BumpMe();
					OnRegistrationUpdated();
				}
			}
		}
		
		// Spawn plate entities from m_Plates PointInfo array (direct positioning system)
		if (m_Plates && m_Plates.Count() > 0)
		{
			Resource resource = Resource.Load(m_LicensePlatePrefab);
			if (!resource.IsValid()) return;
			
			EntitySpawnParams params();
			params.Parent = owner;
			params.TransformMode = ETransformMode.LOCAL;
			
			for (int i = 0; i < m_Plates.Count(); i++)
			{
				auto plate = m_Plates[i];
				GetPositionFromPoint(i, params.Transform);
				
				plate.m_Object = EL_LicensePlateEntity.Cast(GetGame().SpawnEntityPrefabLocal(resource, owner.GetWorld(), params));
				Vehicle veh = Vehicle.Cast(owner);
				
				veh.AddChild(plate.m_Object, -1, EAddChildFlags.RECALC_LOCAL_TRANSFORM);
				plate.m_Object.m_LicensePlateManager = this;
			}
		}
	}
	
	//! Register a plate entity spawned by external system (e.g., vehicle slot system)
	void RegisterPlateEntity(EL_LicensePlateEntity plate)
	{
		if (!plate) return;
		if (m_aRegisteredPlates.Find(plate) == -1)
			m_aRegisteredPlates.Insert(plate);
		plate.UpdateText();
	}
	
	//! Unregister a plate entity (called on entity destruction)
	void UnregisterPlateEntity(EL_LicensePlateEntity plate)
	{
		if (!plate) return;
		m_aRegisteredPlates.RemoveItem(plate);
	}
	
	//! Get the current license plate registration string
	string GetRegistration()
	{
		return m_Registration;
	}
	
	//! Set a specific registration (e.g., restored from garage)
	void SetRegistration(string registration)
	{
		m_Registration = registration;
		Replication.BumpMe();
		OnRegistrationUpdated();
	}
	
	//! Called when m_Registration replicates from server to clients
	void OnRegistrationUpdated()
	{
		// Update manager-spawned plates
		if (m_Plates)
		{
			foreach (auto plate : m_Plates)
			{
				if (plate.m_Object)
					plate.m_Object.UpdateText();
			}
		}
		
		// Update slot-spawned (registered) plates
		foreach (EL_LicensePlateEntity regPlate : m_aRegisteredPlates)
		{
			if (regPlate)
				regPlate.UpdateText();
		}
	}
	
	override void OnPostInit(IEntity owner)
	{		
		SetEventMask(owner, EntityEvent.INIT);
		owner.SetFlags(EntityFlags.ACTIVE, false);
	}
	
	//! Cleanup all plate entities before the vehicle is destroyed
	override void OnDelete(IEntity owner)
	{
		// Cleanup manager-spawned plates
		if (m_Plates)
		{
			foreach (auto plate : m_Plates)
			{
				if (plate.m_Object)
					plate.m_Object.Cleanup();
			}
		}
		
		// Cleanup slot-registered plates (ReloadZ)
		foreach (EL_LicensePlateEntity regPlate : m_aRegisteredPlates)
		{
			if (regPlate)
				regPlate.Cleanup();
		}
		m_aRegisteredPlates.Clear();
		
		super.OnDelete(owner);
	}
	
	bool GetPositionFromPoint(int index, out vector transform[4])
	{
		Math3D.MatrixIdentity4(transform);
		BaseContainer source;
		GenericEntity entity = GetOwner();
		BaseContainerList list;
		
		source = GetComponentSource(entity);
		if (!source) return false;
		
		list = source.GetObjectArray("m_Plates");
		if (!list) return false;

		source = list.Get(index);
		if (!source) return false;

		vector position;
		source.Get("Offset", position);
		
		vector rotation;
		source.Get("Angles", rotation);

		transform[3] = position;
		
		Math3D.AnglesToMatrix(rotation, transform);

		return true;
	}
};
