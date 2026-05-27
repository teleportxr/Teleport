#include "AvatarManager.h"

#include "TeleportCore/ErrorHandling.h"

using nlohmann::json;

namespace teleport
{
	namespace client
	{
		namespace
		{
			constexpr const char *kSignalTypeAvatarPolicy = "avatar-policy";
			constexpr const char *kSignalTypeAvatarOffer  = "avatar-offer";
			constexpr const char *kSignalTypeAvatarResult = "avatar-result";
			constexpr const char *kSignalTypeAvatarRevoke = "avatar-revoke";

			std::string Envelope(const char *type, const json &content)
			{
				json j = {
					{ "teleport-signal-type", type },
					{ "content",              content }
				};
				return j.dump();
			}

			//! Default policy callback: reply have_avatar=false, allowing the
			//! server to fall back to its default avatar (or to reject).
			void DefaultPolicyCallback(const core::AvatarPolicy &policy, AvatarManager::ReplyFn reply)
			{
				core::AvatarOffer offer;
				offer.policyId   = policy.policyId;
				offer.haveAvatar = false;
				reply(offer);
			}
		}

		AvatarManager::AvatarManager(avs::uid srvID, SendFn fn)
			: serverID(srvID)
			, sendFn(std::move(fn))
			, onAvatarPolicy(&DefaultPolicyCallback)
		{
		}

		void AvatarManager::SetOnAvatarPolicy(PolicyCallback cb)
		{
			std::lock_guard lock(mutex);
			onAvatarPolicy = cb ? std::move(cb) : PolicyCallback(&DefaultPolicyCallback);
		}

		void AvatarManager::SetOnAvatarResult(ResultCallback cb)
		{
			std::lock_guard lock(mutex);
			onAvatarResult = std::move(cb);
		}

		void AvatarManager::SetOnAvatarRevoke(RevokeCallback cb)
		{
			std::lock_guard lock(mutex);
			onAvatarRevoke = std::move(cb);
		}

		bool AvatarManager::HandleSignalingMessage(const std::string &msg)
		{
			json parsed;
			try
			{
				parsed = json::parse(msg);
			}
			catch (const std::exception &)
			{
				return false;
			}
			if (!parsed.is_object() || !parsed.contains("teleport-signal-type"))
				return false;
			const auto &typeField = parsed.at("teleport-signal-type");
			if (!typeField.is_string())
				return false;
			const std::string type = typeField.get<std::string>();
			const json content = parsed.contains("content") ? parsed.at("content") : json::object();
			if (type == kSignalTypeAvatarPolicy)
			{
				HandlePolicy(content);
				return true;
			}
			if (type == kSignalTypeAvatarResult)
			{
				HandleResult(content);
				return true;
			}
			if (type == kSignalTypeAvatarRevoke)
			{
				HandleRevoke(content);
				return true;
			}
			return false;
		}

		void AvatarManager::HandlePolicy(const json &content)
		{
			core::AvatarPolicy policy;
			try
			{
				policy = content.get<core::AvatarPolicy>();
			}
			catch (const std::exception &e)
			{
				TELEPORT_INTERNAL_CERR("avatar-policy parse failed for server {}: {}", serverID, e.what());
				return;
			}
			PolicyCallback cb;
			{
				std::lock_guard lock(mutex);
				currentPolicy = policy;
				cb = onAvatarPolicy;
			}
			TELEPORT_INTERNAL_COUT(Default, "avatar-policy <- server {} policy_id={}", serverID, policy.policyId);
			ReplyFn reply = [this](const core::AvatarOffer &offer) { SendOffer(offer); };
			if (cb)
				cb(policy, reply);
			else
				DefaultPolicyCallback(policy, reply);
		}

		void AvatarManager::HandleResult(const json &content)
		{
			core::AvatarResult result;
			try
			{
				result = content.get<core::AvatarResult>();
			}
			catch (const std::exception &e)
			{
				TELEPORT_INTERNAL_CERR("avatar-result parse failed for server {}: {}", serverID, e.what());
				return;
			}
			ResultCallback cb;
			{
				std::lock_guard lock(mutex);
				lastResult = result;
				cb = onAvatarResult;
			}
			TELEPORT_INTERNAL_COUT(Default, "avatar-result <- server {} policy_id={} status={}"
				, serverID, result.policyId, result.status);
			if (cb)
				cb(result);
		}

		void AvatarManager::HandleRevoke(const json &content)
		{
			core::AvatarRevoke revoke;
			try
			{
				revoke = content.get<core::AvatarRevoke>();
			}
			catch (const std::exception &e)
			{
				TELEPORT_INTERNAL_CERR("avatar-revoke parse failed for server {}: {}", serverID, e.what());
				return;
			}
			RevokeCallback cb;
			{
				std::lock_guard lock(mutex);
				// Server has withdrawn the policy; drop cached state so any
				// future offer cannot be matched against it.
				currentPolicy.reset();
				lastOffer.reset();
				lastResult.reset();
				cb = onAvatarRevoke;
			}
			TELEPORT_INTERNAL_COUT(Default, "avatar-revoke <- server {} policy_id={} reason={}"
				, serverID, revoke.policyId, revoke.reason);
			if (cb)
				cb(revoke);
		}

		void AvatarManager::SendOffer(const core::AvatarOffer &offer)
		{
			{
				std::lock_guard lock(mutex);
				lastOffer = offer;
			}
			if (!sendFn)
				return;
			json content = offer;
			TELEPORT_INTERNAL_COUT(Default, "avatar-offer  -> server {} policy_id={} have_avatar={}"
				, serverID, offer.policyId, offer.haveAvatar);
			sendFn(Envelope(kSignalTypeAvatarOffer, content));
		}

		void AvatarManager::SendRevoke(const core::AvatarRevoke &revoke)
		{
			if (!sendFn)
				return;
			json content = revoke;
			TELEPORT_INTERNAL_COUT(Default, "avatar-revoke -> server {} policy_id={}", serverID, revoke.policyId);
			sendFn(Envelope(kSignalTypeAvatarRevoke, content));
		}

		bool AvatarManager::HasCurrentPolicy() const
		{
			std::lock_guard lock(mutex);
			return currentPolicy.has_value();
		}

		std::optional<core::AvatarPolicy> AvatarManager::GetCurrentPolicy() const
		{
			std::lock_guard lock(mutex);
			return currentPolicy;
		}

		std::optional<core::AvatarOffer> AvatarManager::GetLastOffer() const
		{
			std::lock_guard lock(mutex);
			return lastOffer;
		}

		std::optional<core::AvatarResult> AvatarManager::GetLastResult() const
		{
			std::lock_guard lock(mutex);
			return lastResult;
		}
	}
}
