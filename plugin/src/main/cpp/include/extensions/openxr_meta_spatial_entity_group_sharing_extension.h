/**************************************************************************/
/*  openxr_meta_spatial_entity_group_sharing_extension.h                 */
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

#pragma once

#include <openxr/openxr.h>

#include <godot_cpp/classes/open_xr_extension_wrapper.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>

#include "util.h"

using namespace godot;

class OpenXRMetaSpatialEntityGroupSharingExtension : public OpenXRExtensionWrapper {
	GDCLASS(OpenXRMetaSpatialEntityGroupSharingExtension, OpenXRExtensionWrapper);

public:
	Dictionary _get_requested_extensions(uint64_t p_xr_version) override;
	uint64_t _set_system_properties_and_get_next_pointer(void *p_next_pointer) override;
	void _on_instance_created(uint64_t p_instance) override;
	void _on_instance_destroyed() override;
	void _on_session_created(uint64_t p_session) override;
	bool _on_event_polled(const void *p_event) override;

	bool is_group_sharing_supported() const;
	bool share_anchors(const String &p_group_uuid, const Array &p_anchors);
	bool load_group_anchors(const String &p_group_uuid, uint32_t p_max_results = XR_MAX_SPACES_PER_SHARE_REQUEST_META, float p_timeout = 0.0f);

	static OpenXRMetaSpatialEntityGroupSharingExtension *get_singleton();

	OpenXRMetaSpatialEntityGroupSharingExtension();
	~OpenXRMetaSpatialEntityGroupSharingExtension();

protected:
	static void _bind_methods();

private:
	static void _on_group_query_completed(const Vector<XrSpaceQueryResultFB> &p_results, void *p_userdata);
	static bool _parse_uuid(const String &p_uuid, XrUuid &r_uuid);
	void cleanup();

	EXT_PROTO_XRRESULT_FUNC3(xrShareSpacesMETA,
			(XrSession), session,
			(const XrShareSpacesInfoMETA *), info,
			(XrAsyncRequestIdFB *), requestId);

	static OpenXRMetaSpatialEntityGroupSharingExtension *singleton;

	HashMap<String, bool *> request_extensions;
	HashSet<XrAsyncRequestIdFB> share_requests;
	XrSystemSpatialEntitySharingPropertiesMETA sharing_properties{
		XR_TYPE_SYSTEM_SPATIAL_ENTITY_SHARING_PROPERTIES_META,
		nullptr,
		XR_FALSE,
	};
	XrSystemSpatialEntityGroupSharingPropertiesMETA group_sharing_properties{
		XR_TYPE_SYSTEM_SPATIAL_ENTITY_GROUP_SHARING_PROPERTIES_META,
		nullptr,
		XR_FALSE,
	};
	bool meta_spatial_entity_sharing_ext = false;
	bool meta_spatial_entity_group_sharing_ext = false;
	bool system_supports_group_sharing = false;
};
