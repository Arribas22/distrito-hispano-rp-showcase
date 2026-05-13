[ComponentEditorProps(category: "EveronLife/Core/Npcs", description: "Makes the NPC invincible by disabling damage handling")]
class EL_NpcInvincibleComponentClass : ScriptComponentClass
{
}

class EL_NpcInvincibleComponent : ScriptComponent
{
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	override void EOnInit(IEntity owner)
	{
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(owner.FindComponent(SCR_DamageManagerComponent));
		if (dmg)
			dmg.EnableDamageHandling(false);
	}
}
