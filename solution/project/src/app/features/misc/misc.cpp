#include "misc.hpp"

#include "../notifs/notifs.hpp"
#include "../vis_utils/vis_utils.hpp"
#include "../trace_utils/trace_utils.hpp"
#include "../game_movement/move_pred.hpp"
#include "../player_data/player_data.hpp"
#include "../cfg.hpp"

namespace data_centers
{
	static const std::vector<std::string> europe{ "ams", "fra", "lhr", "mad", "par", "sto", "sto2", "waw", "lux", "lux1", "lux2" };
	static const std::vector<std::string> na{ "atl", "eat", "mwh", "iad", "lax", "okc", "ord", "sea" };
	static const std::vector<std::string> sa{ "gru", "lim", "scl" };
	static const std::vector<std::string> asia{ "bom", "dxb", "gnrt", "hkg", "maa", "man", "sgp", "tyo", "tyo2", "tyo1" };
	static const std::vector<std::string> australia{ "syd", "vie" };
	static const std::vector<std::string> africa{ "jnb" };
}

//MAKE_SIG(UTIL_EntitiesInSphere, mem::findBytes("server.dll", "40 53 48 83 EC ? 48 8B 05 ? ? ? ? 49 8B D8 4C 8B C1"));
//MAKE_SIG(CTFWeaponBase_DeflectProjectiles_UTIL_EntitiesInSphere_Call, mem::findBytes("server.dll", "E8 ? ? ? ? 48 8B CF 48 63 D8").offset(5));
//
//MAKE_HOOK(
//	UTIL_EntitiesInSphere,
//	s::UTIL_EntitiesInSphere.get(),
//	int,
//	const vec3 &center, float radius, void *pEnum)
//{
//	int result{ CALL_ORIGINAL(center, radius, pEnum) };
//
//	if (reinterpret_cast<uintptr_t>(_ReturnAddress()) == s::CTFWeaponBase_DeflectProjectiles_UTIL_EntitiesInSphere_Call.get()) {
//		misc->center = center;
//		misc->radius = radius;
//	}
//
//	/*	if (!center.isZero()) {
//		render_utils::RenderWireframeSphere(center, radius, 20, 20, Color{ 255, 0, 0, 255 }, false);
//	}
//
//	if (C_TFPlayer *const local{ ec->getLocal() }) {
//		if (C_TFWeaponBase *const weapon{ ec->getWeapon() }) {
//			const vec3 eye_angles{ local->m_angEyeAngles() };
//			const vec3 eye_position{ local->GetEyePos() };
//
//			vec3 forward{};
//			math::angleVectors(eye_angles, &forward);
//			const float deflection_radius{ tf_utils::attribHookValue(1.0f, "deflection_size_multiplier", weapon) * 128.f };
//			vec3 center { (forward * deflection_radius) + eye_position };
//
//			render_utils::RenderWireframeSphere(center, deflection_radius, 20, 20, Color{ 0, 255, 0, 255 }, false);
//		}
//	}*/
//
//	return result;
//}

void Misc::bhop(CUserCmd *const cmd)
{
	if (!cfg::misc_bhop || !cmd) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local
		|| local->deadflag()
		|| (local->m_nWaterLevel() > WL_Feet)
		|| (local->m_MoveType() != MOVETYPE_WALK)) {
		return;
	}
	
	if (cmd->buttons & IN_JUMP)
	{
		if (local->m_fFlags() & FL_ONGROUND) {
			cmd->buttons |= IN_JUMP;
		}

		else
		{
			const char *const jump_btn_str{ i::engine->Key_LookupBinding("+jump") };
			const ButtonCode_t jump_btn_code{ i::input_system->StringToButtonCode(jump_btn_str) };
			const int jump_btn_vkey{ i::input_system->ButtonCodeToVirtualKey(jump_btn_code) };

			if (!input_tick->getKey(jump_btn_vkey).pressed) {
				cmd->buttons &= ~IN_JUMP;
			}
		}
	}
}

void Misc::autoStrafe(CUserCmd *const cmd)
{
	if (!cfg::auto_strafe_active || !cmd) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local
		|| local->deadflag()
		|| (local->m_fFlags() & FL_ONGROUND)
		|| (local->m_nWaterLevel() > WL_Feet)
		|| (local->m_MoveType() != MOVETYPE_WALK)
		|| (!(tf_globals::last_cmd.buttons & IN_JUMP) && (cmd->buttons & IN_JUMP))) {
		return;
	}

	if (cfg::auto_strafe_mode == 0 && !(cmd->buttons & (IN_MOVELEFT | IN_MOVERIGHT | IN_FORWARD | IN_BACK))) {
		return;
	}

	if (cfg::auto_strafe_mode == 1)
	{
		if (!(cmd->buttons & (IN_MOVELEFT | IN_MOVERIGHT | IN_FORWARD | IN_BACK))) {
			cmd->forwardmove = 450.0f;
		}
	}

	const float f_move{ cmd->forwardmove };
	const float s_move{ cmd->sidemove };

	vec3 view_forward{};
	vec3 view_right{};
	math::angleVectors(i::engine->GetViewAngles(), &view_forward, &view_right, nullptr);

	view_forward.z = 0.0f;
	view_right.z = 0.0f;

	view_forward.normalize();
	view_right.normalize();

	vec3 cur_dir{};
	math::vectorAngles(local->m_vecVelocity(), cur_dir);

	vec3 wish_dir{};
	math::vectorAngles({ (view_forward.x * f_move) + (view_right.x * s_move), (view_forward.y * f_move) + (view_right.y * s_move), 0.0f }, wish_dir);

	const float dir_delta{ math::normalizeAngle(wish_dir.y - cur_dir.y) };
	const float delta_scale{ math::remap(cfg::auto_strafe_smooth, 0.0f, 1.0f, 0.9f, 1.0f) };
	const float rotation{ math::degToRad((dir_delta > 0.0f ? -90.0f : 90.f) + (dir_delta * delta_scale)) };

	const float rot_cos{ cosf(rotation) };
	const float rot_sin{ sinf(rotation) };

	cmd->forwardmove = (rot_cos * f_move) - (rot_sin * s_move);
	cmd->sidemove = (rot_sin * f_move) + (rot_cos * s_move);
}

void Misc::tauntSpin(CUserCmd *const cmd)
{
	if (!cmd || !cfg::misc_taunts_slide) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local || local->deadflag() || !local->InCond(TF_COND_TAUNTING)) {
		return;
	}

	Vector angles{ cmd->viewangles };
	float final_pitch{ angles.x };

	if (cfg::misc_taunts_control && local->m_bAllowMoveDuringTaunt())
	{
		const float backwardsPitch{ local->m_bTauntForceMoveForward() ? 91.f : 0.f};
		const float forwardsPitch{ (cmd->buttons & IN_FORWARD) ? 0.0f : 90.f };

		final_pitch = (cmd->buttons & IN_BACK) ? backwardsPitch : forwardsPitch;

		if (cmd->buttons & IN_MOVELEFT) {
			cmd->sidemove = -450.0f;
		}

		else if (cmd->buttons & IN_MOVERIGHT) {
			cmd->sidemove = 450.0f;
		}
	}
	
	static float taunt_spin_yaw{ angles.y };

	if (cfg::misc_taunts_follow_camera) {
		angles.y = i::engine->GetViewAngles().y;
	}

	if (cfg::misc_taunts_spin)
	{
		const float speed{ fabsf(cfg::misc_taunts_spin_speed) };
		const float time{ i::global_vars->curtime };
		const float interval{ i::global_vars->interval_per_tick * math::pi() * 2.0f };

		float yaw{ cfg::misc_taunts_spin_speed };

		auto randomFloat = [](float min, float max) {
			return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
		};

		auto triangleWave = [](float x, float t, float a)
		{
			if (t == 0.0f) {
				return x;
			}

			const float phase{ fmod(x, t) / t };
			const float value{ (2.0f * abs(2.f * (phase - 0.5f)) - 1.0f) * a };

			return value;
		};

		switch (cfg::misc_taunts_spin_mode)
		{
			case 1: {
				yaw = randomFloat(-speed, speed);
				break;
			}

			case 2: {
				yaw = sinf(time) * cfg::misc_taunts_spin_speed;
				break;
			}

			case 3: {
				yaw = triangleWave(time, interval, speed);
				break;
			}

			default: {
				break;
			}
		}

		angles.y = math::normalizeAngle(taunt_spin_yaw += yaw);
	}

	else {
		taunt_spin_yaw = angles.y;
	}

	tf_utils::fixMovement(cmd, angles);

	cmd->viewangles.y = angles.y;
	cmd->viewangles.x = final_pitch;
}

void Misc::sentryWiggler(CUserCmd *const cmd)
{
	if (!cfg::auto_sentry_wiggler || !cmd) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local || local->deadflag()) {
		return;
	}
	
	C_ObjectSentrygun *const sentry{ local->m_hGroundEntity().Get()->cast<C_ObjectSentrygun>() };

	if (!sentry || sentry->GetClassId() != class_ids::CObjectSentrygun) {
		return;
	}

	if (!cfg::auto_sentry_wiggler_self_rodeo && sentry->m_hBuilder().Get() == local) {
		return;
	}

	vec3 muzzle_position{};

	if (sentry->m_iUpgradeLevel() < 2) {
		vec3 ang{};
		sentry->GetAttachment(sentry->LookupAttachment("muzzle"), muzzle_position, ang);
	}

	else
	{
		vec3 ang{};

		vec3 left_muzzle_position{};
		sentry->GetAttachment(sentry->LookupAttachment("muzzle_l"), left_muzzle_position, ang);

		vec3 right_muzzle_position{};
		sentry->GetAttachment(sentry->LookupAttachment("muzzle_r"), right_muzzle_position, ang);

		muzzle_position = (left_muzzle_position + right_muzzle_position) * 0.5f;
	}

	const vec3 sentry_mins{ sentry->m_vecMins() };
	const vec3 sentry_maxs{ sentry->m_vecMaxs() };
	const vec3 sentry_center{ sentry->GetRenderCenter() };
	const vec3 sentry_origin{ sentry->GetAbsOrigin() };

	const float top{ sentry_origin.z + sentry_maxs.z };

	muzzle_position.z = top;

	const vec3 walk_position{ sentry_center.x, sentry_center.y, top };
	const vec3 muzzle_angle { math::calcAngle(walk_position, muzzle_position) };

	vec3 forward{};
	math::angleVectors(muzzle_angle, &forward);

	const float distance{ sentry_center.distTo(muzzle_position) * cfg::auto_sentry_wiggler_distance};

	vec3 end_position{ walk_position + (forward * -distance) };

	end_position.x = std::clamp(end_position.x, sentry_center.x + sentry_mins.x, sentry_center.x + sentry_maxs.x);
	end_position.y = std::clamp(end_position.y, sentry_center.y + sentry_mins.y, sentry_center.y + sentry_maxs.y);
	end_position.z = top;

	tf_utils::walkTo(cmd, local->GetAbsOrigin(), end_position, 0.3f);
}

void Misc::fastStop(CUserCmd *const cmd)
{
	if (!cfg::misc_faststop || !cmd) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local
		|| local->deadflag()
		|| local->m_MoveType() != MOVETYPE_WALK
		|| !(local->m_fFlags() & FL_ONGROUND)
		|| fabsf(cmd->forwardmove) > 0.0f
		|| fabsf(cmd->sidemove) > 0.0f) {
		return;
	}

	MovePred pred{};

	if (!pred.init(local)) {
		return;
	}

	C_BaseEntity *const world{ i::ent_list->GetClientEntity(0)->cast<C_BaseEntity>() };

	if (world) {
		pred.player.SetGroundEntity(world);
	}

	pred.game_movement.Friction();

	const vec3 vel{ pred.player.GetAbsVelocity() };
	if (vel.length2D() < 10.0f) {
		return;
	}

	vec3 dir{};
	math::vectorAngles(vel, dir);

	dir.y = i::engine->GetViewAngles().y - dir.y;

	vec3 forward{};
	math::angleVectors(dir, &forward);

	const vec3 negated_dir{ forward * -vel.length() };

	cmd->forwardmove = negated_dir.x;
	cmd->sidemove = negated_dir.y;
}

void Misc::autoTurn(CUserCmd *const cmd)
{
	// abel: hacky way to reset the once flags. idk another way to do this compactly so
	static std::unique_ptr<std::once_flag>
		total_reset{ std::make_unique<std::once_flag>() },
		left_reset{ std::make_unique<std::once_flag>() },
		right_reset{ std::make_unique<std::once_flag>() };

	auto shouldRun = [&]()
	{
		if (!cfg::auto_charge_turn) {
			return false;
		}

		C_TFPlayer *const local{ ec->getLocal() };

		if (!local || local->deadflag()) {
			return false;
		}

		if (!local->InCond(TF_COND_SHIELD_CHARGE)) {
			return false;
		}

		// TODO: check wearables for tide turner

		return true;
	};

	if (!shouldRun())
	{
		std::call_once(*total_reset, [&]() {
			i::engine->ClientCmd_Unrestricted("-left");
			i::engine->ClientCmd_Unrestricted("-right");

			left_reset = std::make_unique<std::once_flag>();
			right_reset = std::make_unique<std::once_flag>();
		});

		return;
	}

	if (cmd->mousedx < 0)
	{
		std::call_once(*left_reset, [&]() {
			i::engine->ClientCmd_Unrestricted("+left");
			i::engine->ClientCmd_Unrestricted("-right");
		});

		right_reset = std::make_unique<std::once_flag>();
	}

	if (cmd->mousedx > 0)
	{
		std::call_once(*right_reset, [&]() {
			i::engine->ClientCmd_Unrestricted("+right");
			i::engine->ClientCmd_Unrestricted("-left");
		});

		left_reset = std::make_unique<std::once_flag>();
	}

	total_reset = std::make_unique<std::once_flag>();
}

void Misc::noisemakerSpam()
{
	if (!cfg::misc_noisemaker_spam || !tf_globals::send_packet) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local || i::global_vars->curtime < local->m_flNextNoiseMakerTime()) {
		return;
	}

	KeyValues *const kv{ new KeyValues("use_action_slot_item_server") };

	i::engine->ServerCmdKeyValues(kv);
}

void Misc::mvmRespawn()
{
	if (!cfg::misc_mvm_instant_respawn) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local || !local->deadflag()) {
		return;
	}

	KeyValues *const kv{ new KeyValues("MVM_Revive_Response") };

	kv->SetString("accepted", "1");

	i::engine->ServerCmdKeyValues(kv);
}

void Misc::autoMvmReadyUp()
{
	if (!cfg::auto_mvm_ready_up) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local) {
		return;
	}

	C_TFGameRulesProxy *game_rules{ ec->getGameRules() };

	if (!game_rules) {
		return;
	}

	if (!game_rules->m_bPlayingMannVsMachine()
		|| !game_rules->m_bInWaitingForPlayers()
		|| game_rules->m_iRoundState() != GR_STATE_BETWEEN_RNDS) {
		return;
	}

	const int local_index{ local->entindex() };

	if (local_index < 0 || local_index >= 100) {
		return;
	}

	if (!game_rules->IsPlayerReady(local_index)) {
		i::engine->ClientCmd_Unrestricted("tournament_player_readystate 1");
	}
}

void Misc::autoDisguise()
{
	if (!cfg::auto_disguise) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local 
		|| local->deadflag() 
		|| local->m_iClass() != TF_CLASS_SPY 
		|| local->InCond(TF_COND_DISGUISED) 
		|| local->InCond(TF_COND_DISGUISING)) {
		return;
	}

	i::engine->ClientCmd_Unrestricted("lastdisguise");
}

int Misc::getRegionPing(const unsigned int pop_id)
{
	if (!cfg::region_selector_active) {
		return 0;
	}

	// https://github.com/ValveSoftware/GameNetworkingSockets/blob/8f4d800b2ac9690bb049d8012135dfedd19488b9/include/steam/steamnetworkingtypes.h#L1792
	std::string region{};
	region.reserve(4);

	region += char(pop_id >> 16U);
	region += char(pop_id >> 8U);
	region += char(pop_id);

	if (const char last_char{ char(pop_id >> 24U) }) {
		region += last_char;
	}

	if (region.empty()) {
		return 0;
	}

	if (cfg::region_selector_disable_europe && std::find(data_centers::europe.begin(), data_centers::europe.end(), region) != data_centers::europe.end()) {
		return 69420;
	}

	if (cfg::region_selector_disable_na && std::find(data_centers::na.begin(), data_centers::na.end(), region) != data_centers::na.end()) {
		return 69420;
	}

	if (cfg::region_selector_disable_sa && std::find(data_centers::sa.begin(), data_centers::sa.end(), region) != data_centers::sa.end()) {
		return 69420;
	}

	if (cfg::region_selector_disable_asia && std::find(data_centers::asia.begin(), data_centers::asia.end(), region) != data_centers::asia.end()) {
		return 69420;
	}

	if (cfg::region_selector_disable_australia && std::find(data_centers::australia.begin(), data_centers::australia.end(), region) != data_centers::australia.end()) {
		return 69420;
	}

	if (cfg::region_selector_disable_africa && std::find(data_centers::africa.begin(), data_centers::africa.end(), region) != data_centers::africa.end()) {
		return 69420;
	}

	return 1;
}

void Misc::paintFOVCircle()
{
	if (!cfg::fov_circle_active) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local || local->deadflag() || !local->InFirstPersonView()) {
		return;
	}

	C_TFWeaponBase *const weapon{ ec->getWeapon() };

	if (!weapon) {
		return;
	}

	const int weapon_id{ weapon->GetWeaponID() };

	if (weapon_id == TF_WEAPON_MEDIGUN
		|| weapon_id == TF_WEAPON_BUILDER
		|| weapon_id == TF_WEAPON_PDA_ENGINEER_BUILD
		|| weapon_id == TF_WEAPON_PDA_ENGINEER_DESTROY
		|| weapon_id == TF_WEAPON_PDA_SPY) {
		return;
	}

	float aimbot_fov{};

	if (tf_utils::isWeaponHitscan(weapon))
	{
		if ((cfg::fov_circle_without_aimbot || cfg::aimbot_hitscan_active) && cfg::aimbot_hitscan_mode == 0) {
			aimbot_fov = cfg::aimbot_hitscan_fov;
		}
	}

	if (tf_utils::isWeaponMelee(weapon))
	{
		if ((cfg::fov_circle_without_aimbot || cfg::aimbot_melee_active) && cfg::aimbot_melee_mode == 0) {
			aimbot_fov = cfg::aimbot_melee_fov;
		}
	}

	if (tf_utils::isWeaponProjectile(weapon))
	{
		if ((cfg::fov_circle_without_aimbot || cfg::aimbot_projectile_active) && cfg::aimbot_projectile_mode == 0) {
			aimbot_fov = cfg::aimbot_projectile_fov;
		}
	}

	const float radius{ tanf(math::degToRad(aimbot_fov) * 0.5f) / tanf(math::degToRad(local->GetFOV()) * 0.5f) * draw->getScreenSize().x };

	Color color{ cfg::fov_circle_color };

	if (cfg::fov_circle_style == 0) {
		draw->circle(draw->getScreenSize() * 0.5f, radius, color);
	}

	if (cfg::fov_circle_style == 1)
	{
		static float rotation{};

		rotation += cfg::fov_circle_spin_speed * i::global_vars->frametime;

		if (rotation >= 2.0f * 3.14f) {
			rotation -= 2.0f * 3.14f;
		}

		const int segments{ 100 };

		for (int n{}; n < segments; n++)
		{
			if (n % cfg::fov_circle_spacing) {
				continue;
			}

			const float segment_angle{ 2.0f * 3.14f / static_cast<float>(segments) };

			const float angle_0{ n * segment_angle + rotation };
			const float angle_1{ (n + 1) * segment_angle + rotation };

			const vec2 circle_pos{ draw->getScreenSize() * 0.5f };

			const vec2 p0{ circle_pos.x + radius * cosf(angle_0), circle_pos.y + radius * sinf(angle_0) };
			const vec2 p1{ circle_pos.x + radius * cosf(angle_1), circle_pos.y + radius * sinf(angle_1) };

			draw->line(p0, p1, color);
		}
	}
}

void Misc::paintPredPath()
{
	if (!cfg::prediction_path_active || tf_globals::pred_path.size() < 2) {
		return;
	}

	if (i::global_vars->curtime >= tf_globals::pred_path_expire_time) {
		tf_globals::pred_path.clear();
		return;
	}

	if (cfg::prediction_path_style == 0)
	{
		for (size_t n{ 1 }; n < tf_globals::pred_path.size(); n++) {
			render_utils::RenderLine(tf_globals::pred_path[n], tf_globals::pred_path[n - 1], cfg::prediction_path_color, !cfg::prediction_path_no_depth);
		}
	}

	if (cfg::prediction_path_style == 1)
	{
		bool skip{ true };

		for (size_t n{}; n + 1 < tf_globals::pred_path.size(); n++)
		{
			if (skip) {
				skip = false;
				continue;
			}

			skip = true;

			render_utils::RenderLine(tf_globals::pred_path[n], tf_globals::pred_path[n + 1], cfg::prediction_path_color, !cfg::prediction_path_no_depth);
		}
	}

	if (cfg::prediction_path_style == 2)
	{
		for (size_t n{ 1 }; n < tf_globals::pred_path.size(); n++)
		{
			if (n % 3 != 0) {
				continue;
			}

			render_utils::RenderSphere(tf_globals::pred_path[n], 2.0f, 20, 20, cfg::prediction_path_color, !cfg::prediction_path_no_depth);
		}
	}

	if (cfg::prediction_path_style == 3)
	{
		const float arrow_len{ 5.0f };

		for (size_t n{ 1 }; n < tf_globals::pred_path.size(); n++)
		{
			if (n % 3 == 0)
			{
				const vec3 dir{ (tf_globals::pred_path[n - 1] - tf_globals::pred_path[n]).normalized() };

				const vec3 arrow_l{ tf_globals::pred_path[n] + dir * arrow_len + vec3{ -dir.y, dir.x, 0.0f } * arrow_len };
				const vec3 arrow_r{ tf_globals::pred_path[n] + dir * arrow_len + vec3{ dir.y, -dir.x, 0.0f } * arrow_len };

				render_utils::RenderLine(tf_globals::pred_path[n], arrow_l, cfg::prediction_path_color, !cfg::prediction_path_no_depth);
				render_utils::RenderLine(tf_globals::pred_path[n], arrow_r, cfg::prediction_path_color, !cfg::prediction_path_no_depth);
			}
		}
	}
	if (cfg::prediction_path_style == 4) {
		constexpr float spike_length = 10.0f;

		for (size_t n = 1; n < tf_globals::pred_path.size(); ++n) {
			const vec3& point = tf_globals::pred_path[n];
			const vec3& previous_point = tf_globals::pred_path[n - 1];

			render_utils::RenderLine(point, previous_point, cfg::prediction_path_color, !cfg::prediction_path_no_depth);

			if (n % 4 == 0) {
				vec3 direction = point - previous_point;
				direction.normalize();

				vec3 perpendicular_direction = vec3(-direction.y, direction.x, 0);
				perpendicular_direction.normalize();

				const vec3& spike_start = point;
				const vec3& spike_end = point + perpendicular_direction * spike_length;

				render_utils::RenderLine(spike_start, spike_end, cfg::prediction_path_color, !cfg::prediction_path_no_depth);
			}
		}
	}
}

void Misc::paintSniperSightlines()
{
	if (cfg::sniper_sight_lines != 2) {
		return;
	}

	C_TFPlayer *const local{ ec->getLocal() };

	if (!local) {
		return;
	}

	for (const EntityHandle_t ehandle : ec->getGroup(ECGroup::PLAYERS_ALL))
	{
		IClientEntity *const ent{ i::ent_list->GetClientEntityFromHandle(ehandle) };

		if (!ent) {
			continue;
		}

		C_TFPlayer *const player{ ent->cast<C_TFPlayer>() };

		if (player == local
			|| player->deadflag()
			|| player->m_iClass() != TF_CLASS_SNIPER) {
			continue;
		}

		C_TFWeaponBase *const weapon{ player->m_hActiveWeapon()->cast<C_TFWeaponBase>() };

		if (!weapon) {
			continue;
		}

		if (weapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_CLASSIC)
		{
			if (!weapon->cast<C_TFSniperRifleClassic>()->m_bCharging()) {
				continue;
			}
		}

		else
		{
			if (!player->InCond(TF_COND_ZOOMED)) {
				continue;
			}
		}

		vec3 shoot_dir{};
		math::angleVectors(player->m_angEyeAngles(), &shoot_dir);

		const vec3 shoot_pos{ player->m_vecOrigin() + vec3{ 0.0f, 0.0f, ((player->m_fFlags() & FL_DUCKING) ? 45.0f : 75.0f) * player->m_flModelScale() } };

		trace_t tr{};
		tf_utils::trace(shoot_pos, shoot_pos + (shoot_dir * 8192.0f), &trace_filters::world, &tr, MASK_SHOT);

		render_utils::RenderLine(shoot_pos, tr.endpos, vis_utils->getEntColor(player), true);
	}
}

struct Triangle
{
	Vector v0, v1, v2;
};

float Cross2D(const Vector& a, const Vector& b, const Vector& c)
{
	// Compute (b - a) x (c - a) in 2D.
	float abx = b.x - a.x;
	float aby = b.y - a.y;
	float acx = c.x - a.x;
	float acy = c.y - a.y;
	return abx * acy - aby * acx;
}

// Computes a 2D convex hull using a simple Graham scan.
// Assumes all points lie roughly in the same plane (ignores Z).
std::vector<Vector> ComputeConvexHull(std::vector<Vector> points)
{
	std::vector<Vector> hull;
	if ( points.size() < 3 )
		return points;

	// 1. Find the pivot: the point with the lowest Y (and then lowest X if tied).
	Vector pivot = *std::min_element(points.begin(), points.end(),
									 [ ](const Vector& a, const Vector& b) {
		return ( a.y < b.y ) || ( a.y == b.y && a.x < b.x );
	});

	// 2. Sort the points by polar angle with respect to the pivot.
	std::sort(points.begin(), points.end(),
			  [ pivot ](const Vector& a, const Vector& b) {
		float angleA = atan2(a.y - pivot.y, a.x - pivot.x);
		float angleB = atan2(b.y - pivot.y, b.x - pivot.x);
		return angleA < angleB;
	});

	// 3. Process the sorted points to build the convex hull.
	for ( const auto& point : points )
	{
		// While the hull has at least two points and the last three points do not make a left turn:
		while ( hull.size() >= 2 && Cross2D(hull[hull.size() - 2], hull[hull.size() - 1], point) <= 0 )
		{
			hull.pop_back();
		}
		hull.push_back(point);
	}

	return hull;
}

// Triangulate the convex hull using a simple triangle fan.
// Returns a list of triangles covering the convex polygon.
std::vector<Triangle> TriangulateHull(const std::vector<Vector>& hull)
{
	std::vector<Triangle> triangles;
	// A valid convex polygon must have at least 3 vertices.
	if ( hull.size() < 3 )
		return triangles;

	// Use the first vertex as the fan center.
	for ( size_t i = 1; i < hull.size() - 1; i++ )
	{
		Triangle tri;
		tri.v0 = hull[0];
		tri.v1 = hull[i];
		tri.v2 = hull[i + 1];
		triangles.push_back(tri);
	}
	return triangles;
}

void Misc::paintSitckyMesh()
{
	auto proj_handles = ec->getGroup(ECGroup::PROJECTILES_ENEMIES);
	const auto& proj_handles_local = ec->getGroup(ECGroup::PROJECTILES_LOCAL);

	proj_handles.insert(proj_handles.end(), proj_handles_local.begin(), proj_handles_local.end());

	if ( proj_handles.empty() )
		return;

	std::unordered_map<int, std::vector<C_TFGrenadePipebombProjectile*>> stickyGroups;

	for ( const EntityHandle_t ehandle : proj_handles )
	{
		IClientEntity* const ent = i::ent_list->GetClientEntityFromHandle(ehandle);

		if ( !ent || ent->GetClassId() != class_ids::CTFGrenadePipebombProjectile )
			continue;

		C_TFGrenadePipebombProjectile* const sticky { ent->cast<C_TFGrenadePipebombProjectile>() };

		if ( !sticky || sticky->m_iType() != TF_GL_MODE_REMOTE_DETONATE || !sticky->m_bTouched() )
			continue;

		auto thrower = sticky->m_hThrower().ToInt();

		stickyGroups[thrower].push_back(sticky);
	}

	for ( const auto& groupPair : stickyGroups )
	{
		const auto& idx = groupPair.first;
		const auto& group = groupPair.second;

		std::vector<Vector> sticky_positions;

		for ( auto& sticky : group )
		{
			const Vector sticky_pos = sticky->GetCenter();
			sticky_positions.push_back(sticky_pos);
		}

		if ( sticky_positions.size() < 3 )
			continue;

		std::vector<Vector> hull = ComputeConvexHull(sticky_positions);

		if ( hull.size() < 3 )
			continue;

		auto const thrower { i::ent_list->GetClientEntity(idx)->cast<C_TFPlayer>() };

		Color color_triangle { vis_utils->getEntColor(thrower) };
		color_triangle.a = 110;

		std::vector<Triangle> triangles = TriangulateHull(hull);

		for ( const Triangle& tri : triangles )
			render_utils::RenderTriangle(tri.v0, tri.v1, tri.v2, color_triangle, true);

		for ( size_t i = 0; i < hull.size(); i++ )
		{
			size_t next = ( i + 1 ) % hull.size();
			render_utils::RenderLine(hull[i], hull[next], Color(255, 255, 255, 255), true);
		}
	}
}


float UTIL_AngleDiff(float destAngle, float srcAngle)
{
	float delta = destAngle - srcAngle;

	if ( destAngle > srcAngle )
	{
		while ( delta >= 180.0f )
			delta -= 360.0f;
	}
	else
	{
		while ( delta <= -180.0f )
			delta += 360.0f;
	}

	return delta;
}

QAngle GetAngularVelocity(C_TFPlayer* player, QAngle& prevAngles)
{
	QAngle currentAngles = player->GetAbsAngles();
	QAngle delta;
	delta.x = UTIL_AngleDiff(currentAngles.x, prevAngles.x);
	delta.y = UTIL_AngleDiff(currentAngles.y, prevAngles.y);
	delta.z = 0;

	// Get the frame time (time elapsed since the last frame).
	float frametime = i::global_vars->frametime;

	QAngle angularVelocity(0, 0, 0);
	if ( frametime > 0 )
	{
		// Compute angular velocity in degrees per second.
		angularVelocity = delta / frametime;
	}

	// Update previous angles for the next frame.
	prevAngles = currentAngles;

	return angularVelocity;
}

QAngle ComputeAngularVelocity(const QAngle& current, const QAngle& previous, float dt)
{
	QAngle delta;
	delta.x = UTIL_AngleDiff(current.x, previous.x);
	delta.y = UTIL_AngleDiff(current.y, previous.y);
	delta.z = 0;

	//QAngle angularVelocity(0, 0, 0);
	//if ( dt > 0 )
	//{
	//	angularVelocity = delta / dt;
	//}
	return delta;
}

//void Misc::paintViewVelocity()
//{
//	for ( const EntityHandle_t ehandle : ec->getGroup(ECGroup::PLAYERS_TEAMMATES) )
//	{
//		C_TFPlayer* const pl { i::ent_list->GetClientEntityFromHandle(ehandle)->cast<C_TFPlayer>() };
//
//		if ( !pl || pl->IsDormant() || pl->deadflag() )
//			continue;
//
//		const size_t num_recs { 66ull };
//
//		if ( player_data->getNumValidRecords(pl) >= num_recs )
//		{
//			QAngle avgAngleVel = {};
//
//			const LagRecord* prevRecord = player_data->getRecord(pl, 0);
//			if ( !prevRecord )
//				return;
//
//			size_t n = 1;
//
//			for ( ; n < num_recs; n++ )
//			{
//				const LagRecord* currentRecord = player_data->getRecord(pl, n);
//				if ( !currentRecord )
//					continue;
//
//				float dt = currentRecord->sim_time - prevRecord->sim_time;
//				if ( dt <= 0 )
//					dt = i::global_vars->frametime;
//
//				avgAngleVel += ComputeAngularVelocity(prevRecord->abs_angles, currentRecord->abs_angles, dt);
//
//				prevRecord = currentRecord;
//			}
//			avgAngleVel /= n;
//
//			const Color clr0 = { 255, 117, 0, 255 };
//			const Color clr1 = { 0, 255, 0, 255 };
//			const float lineLength = 78.75f;
//
//			int ticks = tf_utils::timeToTicks(1.0f);
//			for ( int i = 0; i < ticks; i++ )
//			{
//				const auto& velAng = avgAngleVel;
//
//				QAngle ang = { pl->m_angEyeAngles() + ( velAng * i::global_vars->interval_per_tick ) };
//				math::clampAngles(ang);
//
//				vec3 dir {};
//				math::angleVectors(ang, &dir);
//
//				const vec3 pos { pl->m_vecOrigin() + vec3{ 0.0f, 0.0f, ( ( pl->m_fFlags() & FL_DUCKING ) ? 45.0f : 75.0f ) * pl->m_flModelScale() } };
//				const vec3 end_pos = { pos + ( dir * lineLength ) };
//
//				float t = static_cast<float>( i ) / static_cast<float>( ticks - 1 );
//				const Color clr = clr1.lerp(clr0, t);
//
//				render_utils::RenderLine(pos, end_pos, clr, false);
//			}
//		}
//	}
//}
