/**************************************************************************/
/*  openxr_meta_spatial_entity_group_sharing_extension.cpp               */
/**************************************************************************/
/*                       This file is part of:                            */
/*                              GODOT XR                                  */
/*                      https://godotengine.org                           */
/**************************************************************************/
/* Copyright (c) 2022-present Godot XR contributors (see CONTRIBUTORS.md) */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "extensions/openxr_meta_spatial_entity_group_sharing_extension.h"

#include <godot_cpp/classes/open_xrapi_extension.hpp>
#include <godot_cpp/templates/local_vector.hpp>

#include "classes/openxr_fb_spatial_entity.h"
#include "extensions/openxr_fb_spatial_entity_query_extension.h"

using namespace godot;

OpenXRMetaSpatialEntityGroupSharingExtension *OpenXRMetaSpatialEntityGroupSharingExtension::singleton = nullptr;

OpenXRMetaSpatialEntityGroupSharingExtension *OpenXRMetaSpatialEntityGroupSharingExtension::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(OpenXRMetaSpatialEntityGroupSharingExtension());
	}
	return singleton;
}

OpenXRMetaSpatialEntityGroupSharingExtension::OpenXRMetaSpatialEntityGroupSharingExtension() :
		OpenXRExtensionWrapper() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "An OpenXRMetaSpatialEntityGroupSharingExtension singleton already exists.");

	request_extensions[XR_META_SPATIAL_ENTITY_SHARING_EXTENSION_NAME] = &meta_spatial_entity_sharing_ext;
	request_extensions[XR_META_SPATIAL_ENTITY_GROUP_SHARING_EXTENSION_NAME] = &meta_spatial_entity_group_sharing_ext;
	singleton = this;
}

OpenXRMetaSpatialEntityGroupSharingExtension::~OpenXRMetaSpatialEntityGroupSharingExtension() {
	cleanup();
	singleton = nullptr;
}

void OpenXRMetaSpatialEntityGroupSharingExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_group_sharing_supported"), &OpenXRMetaSpatialEntityGroupSharingExtension::is_group_sharing_supported);
	ClassDB::bind_method(D_METHOD("share_anchors", "group_uuid", "anchors"), &OpenXRMetaSpatialEntityGroupSharingExtension::share_anchors);
	ClassDB::bind_method(D_METHOD("load_group_anchors", "group_uuid", "max_results", "timeout"), &OpenXRMetaSpatialEntityGroupSharingExtension::load_group_anchors, DEFVAL(XR_MAX_SPACES_PER_SHARE_REQUEST_META), DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("get_last_result_code"), &OpenXRMetaSpatialEntityGroupSharingExtension::get_last_result_code);
	ClassDB::bind_method(D_METHOD("get_last_result_string"), &OpenXRMetaSpatialEntityGroupSharingExtension::get_last_result_string);

	ADD_SIGNAL(MethodInfo("openxr_meta_group_anchors_shared", PropertyInfo(Variant::BOOL, "succeeded")));
	ADD_SIGNAL(MethodInfo("openxr_meta_group_anchors_loaded", PropertyInfo(Variant::ARRAY, "anchors")));
}

Dictionary OpenXRMetaSpatialEntityGroupSharingExtension::_get_requested_extensions(uint64_t p_xr_version) {
	Dictionary result;
	for (auto ext : request_extensions) {
		uint64_t value = reinterpret_cast<uint64_t>(ext.value);
		result[ext.key] = (Variant)value;
	}
	return result;
}

uint64_t OpenXRMetaSpatialEntityGroupSharingExtension::_set_system_properties_and_get_next_pointer(void *p_next_pointer) {
	if (meta_spatial_entity_sharing_ext && meta_spatial_entity_group_sharing_ext) {
		group_sharing_properties.next = p_next_pointer;
		group_sharing_properties.supportsSpatialEntityGroupSharing = XR_FALSE;
		sharing_properties.next = &group_sharing_properties;
		sharing_properties.supportsSpatialEntitySharing = XR_FALSE;
		p_next_pointer = &sharing_properties;
	}
	return reinterpret_cast<uint64_t>(p_next_pointer);
}

void OpenXRMetaSpatialEntityGroupSharingExtension::_on_instance_created(uint64_t p_instance) {
	if (!meta_spatial_entity_sharing_ext || !meta_spatial_entity_group_sharing_ext) {
		return;
	}

	GDEXTENSION_INIT_XR_FUNC(xrShareSpacesMETA);
}

void OpenXRMetaSpatialEntityGroupSharingExtension::_on_instance_destroyed() {
	cleanup();
}

void OpenXRMetaSpatialEntityGroupSharingExtension::_on_session_created(uint64_t p_session) {
	system_supports_group_sharing =
			meta_spatial_entity_sharing_ext &&
			meta_spatial_entity_group_sharing_ext &&
			sharing_properties.supportsSpatialEntitySharing == XR_TRUE &&
			group_sharing_properties.supportsSpatialEntityGroupSharing == XR_TRUE;
}

bool OpenXRMetaSpatialEntityGroupSharingExtension::_on_event_polled(const void *p_event) {
	const XrEventDataBuffer *event = static_cast<const XrEventDataBuffer *>(p_event);
	if (event->type != XR_TYPE_EVENT_DATA_SHARE_SPACES_COMPLETE_META) {
		return false;
	}

	const XrEventDataShareSpacesCompleteMETA *share_event = reinterpret_cast<const XrEventDataShareSpacesCompleteMETA *>(p_event);
	if (!share_requests.has(share_event->requestId)) {
		WARN_PRINT("Received unexpected XR_TYPE_EVENT_DATA_SHARE_SPACES_COMPLETE_META");
		return true;
	}

	share_requests.erase(share_event->requestId);
	last_result = share_event->result;
	emit_signal("openxr_meta_group_anchors_shared", XR_SUCCEEDED(share_event->result));
	return true;
}

bool OpenXRMetaSpatialEntityGroupSharingExtension::is_group_sharing_supported() const {
	return system_supports_group_sharing;
}

int64_t OpenXRMetaSpatialEntityGroupSharingExtension::get_last_result_code() const {
	return (int64_t)last_result;
}

String OpenXRMetaSpatialEntityGroupSharingExtension::get_last_result_string() {
	Ref<OpenXRAPIExtension> openxr_api = get_openxr_api();
	String result = openxr_api.is_valid() ? openxr_api->get_error_string(last_result) : String::num_int64((int64_t)last_result);
	if (last_result == XR_ERROR_SPACE_COMPONENT_NOT_ENABLED_FB) {
		result += " (enable SHARABLE first)";
	}
	return result;
}

bool OpenXRMetaSpatialEntityGroupSharingExtension::share_anchors(const String &p_group_uuid, const Array &p_anchors) {
	ERR_FAIL_COND_V_MSG(!is_group_sharing_supported(), false, "XR_META_spatial_entity_group_sharing is not supported.");
	ERR_FAIL_COND_V_MSG(p_anchors.is_empty(), false, "At least one spatial anchor is required.");
	ERR_FAIL_COND_V_MSG(p_anchors.size() > XR_MAX_SPACES_PER_SHARE_REQUEST_META, false, "Too many spatial anchors in one share request.");

	XrUuid group_uuid;
	ERR_FAIL_COND_V_MSG(!_parse_uuid(p_group_uuid, group_uuid), false, vformat("Invalid group UUID: %s", p_group_uuid));

	LocalVector<XrSpace> spaces;
	spaces.resize(p_anchors.size());
	for (int i = 0; i < p_anchors.size(); i++) {
		Ref<OpenXRFbSpatialEntity> entity = p_anchors[i];
		ERR_FAIL_COND_V_MSG(entity.is_null(), false, "Anchor array contains an invalid spatial entity.");
		ERR_FAIL_COND_V_MSG(entity->get_space() == XR_NULL_HANDLE, false, "Anchor array contains a destroyed spatial entity.");
		if (!entity->is_component_enabled(OpenXRFbSpatialEntity::COMPONENT_TYPE_SHARABLE)) {
			last_result = XR_ERROR_SPACE_COMPONENT_NOT_ENABLED_FB;
			ERR_PRINT("Cannot share a spatial entity because COMPONENT_TYPE_SHARABLE isn't enabled.");
			return false;
		}
		spaces[i] = entity->get_space();
	}

	XrShareSpacesRecipientGroupsMETA recipients = {
		XR_TYPE_SHARE_SPACES_RECIPIENT_GROUPS_META,
		nullptr,
		1,
		&group_uuid,
	};
	XrShareSpacesInfoMETA info = {
		XR_TYPE_SHARE_SPACES_INFO_META,
		nullptr,
		(uint32_t)spaces.size(),
		spaces.ptr(),
		reinterpret_cast<const XrShareSpacesRecipientBaseHeaderMETA *>(&recipients),
	};
	XrAsyncRequestIdFB request_id = 0;
	XrResult result = xrShareSpacesMETA((XrSession)get_openxr_api()->get_session(), &info, &request_id);
	last_result = result;
	if (XR_FAILED(result)) {
		WARN_PRINT(vformat("xrShareSpacesMETA failed: %s", get_openxr_api()->get_error_string(result)));
		emit_signal("openxr_meta_group_anchors_shared", false);
		return false;
	}

	share_requests.insert(request_id);
	return true;
}

bool OpenXRMetaSpatialEntityGroupSharingExtension::load_group_anchors(const String &p_group_uuid, uint32_t p_max_results, float p_timeout) {
	ERR_FAIL_COND_V_MSG(!is_group_sharing_supported(), false, "XR_META_spatial_entity_group_sharing is not supported.");
	ERR_FAIL_COND_V_MSG(p_max_results == 0, false, "max_results must be greater than zero.");

	XrUuid group_uuid;
	ERR_FAIL_COND_V_MSG(!_parse_uuid(p_group_uuid, group_uuid), false, vformat("Invalid group UUID: %s", p_group_uuid));

	XrSpaceStorageLocationFilterInfoFB location_filter = {
		XR_TYPE_SPACE_STORAGE_LOCATION_FILTER_INFO_FB,
		nullptr,
		XR_SPACE_STORAGE_LOCATION_CLOUD_FB,
	};
	XrSpaceGroupUuidFilterInfoMETA group_filter = {
		XR_TYPE_SPACE_GROUP_UUID_FILTER_INFO_META,
		&location_filter,
		group_uuid,
	};
	XrSpaceQueryInfoFB query = {
		XR_TYPE_SPACE_QUERY_INFO_FB,
		nullptr,
		XR_SPACE_QUERY_ACTION_LOAD_FB,
		p_max_results,
		(XrDuration)(p_timeout * 1000000000.0f),
		reinterpret_cast<XrSpaceFilterInfoBaseHeaderFB *>(&group_filter),
		nullptr,
	};

	const bool accepted = OpenXRFbSpatialEntityQueryExtension::get_singleton()->query_spatial_entities(
			reinterpret_cast<XrSpaceQueryInfoBaseHeaderFB *>(&query),
			OpenXRMetaSpatialEntityGroupSharingExtension::_on_group_query_completed,
			this);
	return accepted;
}

void OpenXRMetaSpatialEntityGroupSharingExtension::_on_group_query_completed(const Vector<XrSpaceQueryResultFB> &p_results, void *p_userdata) {
	OpenXRMetaSpatialEntityGroupSharingExtension *extension = static_cast<OpenXRMetaSpatialEntityGroupSharingExtension *>(p_userdata);
	Array results;
	results.resize(p_results.size());
	for (int i = 0; i < p_results.size(); i++) {
		results[i] = Ref<OpenXRFbSpatialEntity>(memnew(OpenXRFbSpatialEntity(p_results[i].space, p_results[i].uuid)));
	}
	extension->emit_signal("openxr_meta_group_anchors_loaded", results);
}

bool OpenXRMetaSpatialEntityGroupSharingExtension::_parse_uuid(const String &p_uuid, XrUuid &r_uuid) {
	String normalized = p_uuid.strip_edges().to_lower();
	if (normalized.length() == 32) {
		normalized = normalized.substr(0, 8) + "-" +
				normalized.substr(8, 4) + "-" +
				normalized.substr(12, 4) + "-" +
				normalized.substr(16, 4) + "-" +
				normalized.substr(20, 12);
	}
	if (normalized.length() != 36) {
		return false;
	}

	r_uuid = OpenXRUtilities::string_name_to_uuid(StringName(normalized));
	return OpenXRUtilities::uuid_to_string_name(r_uuid) == StringName(normalized);
}

void OpenXRMetaSpatialEntityGroupSharingExtension::cleanup() {
	share_requests.clear();
	meta_spatial_entity_sharing_ext = false;
	meta_spatial_entity_group_sharing_ext = false;
	system_supports_group_sharing = false;
	last_result = XR_SUCCESS;
	xrShareSpacesMETA_ptr = nullptr;
	sharing_properties.next = nullptr;
	sharing_properties.supportsSpatialEntitySharing = XR_FALSE;
	group_sharing_properties.next = nullptr;
	group_sharing_properties.supportsSpatialEntityGroupSharing = XR_FALSE;
}
