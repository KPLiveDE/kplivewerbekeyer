#include <obs-module.h>
#include <obs-frontend-api.h>

struct output_source_context {
	obs_source_t *source;
	bool rendering;
	int mode;
	obs_source_t *outputSource;
	uint32_t width;
	uint32_t height;
	bool recurring;
	gs_texrender_t *render;
};

static const char *output_source_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("OutputSource");
}

static void output_source_update(void *data, obs_data_t *settings)
{
	struct output_source_context *context = data;
	context->mode = (int)obs_data_get_int(settings, "mode");
}

static void *output_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct output_source_context *context = bzalloc(sizeof(struct output_source_context));
	context->source = source;

	output_source_update(context, settings);
	return context;
}

static void output_source_destroy(void *data)
{
	struct output_source_context *context = data;
	if (context->render) {
		obs_enter_graphics();
		gs_texrender_destroy(context->render);
		obs_leave_graphics();
	}
	if (context->outputSource)
		obs_source_release(context->outputSource);
	bfree(context);
}

static obs_properties_t *output_source_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *ppts = obs_properties_create();
	obs_property_t *mode = obs_properties_add_list(ppts, "mode", obs_module_text("SourceMode"),
							 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, obs_module_text("Program"), 0);
	obs_property_list_add_int(mode, obs_module_text("Preview"), 1);
	return ppts;
}

void output_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "mode", 0);
}

static void output_source_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct output_source_context *context = data;
	if (context->recurring && context->render) {
		gs_texture_t *tex = gs_texrender_get_texture(context->render);
		if (tex) {
			effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
			gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
			gs_effect_set_texture(image, tex);
			while (gs_effect_loop(effect, "Draw"))
				gs_draw_sprite(tex, 0, context->width, context->height);
			return;
		}
	}

	if (context->rendering || context->recurring || !context->outputSource)
		return;

	context->rendering = true;
	obs_source_video_render(context->outputSource);
	context->rendering = false;
}

static uint32_t output_source_getwidth(void *data)
{
	struct output_source_context *context = data;
	return context->width;
}

static uint32_t output_source_getheight(void *data)
{
	struct output_source_context *context = data;
	return context->height;
}

static void check_recursion(obs_source_t *parent, obs_source_t *child, void *data)
{
	UNUSED_PARAMETER(parent);
	struct output_source_context *context = data;
	if (child == context->source) {
		context->recurring = true;
	}
}

static void output_source_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct output_source_context *context = data;
	obs_source_t *source = NULL;
	if (context->mode == 1) {
		source = obs_frontend_get_current_preview_scene();
		if (!source)
			source = obs_frontend_get_current_scene();
	} else {
		source = obs_get_output_source(0);
	}
	if (!source) {
		if (context->outputSource) {
			obs_source_release(context->outputSource);
			context->outputSource = NULL;
			context->recurring = false;
		}
		return;
	}
	context->recurring = false;
	obs_source_enum_active_tree(source, check_recursion, data);
	if (source != context->outputSource) {
		if (context->outputSource)
			obs_source_release(context->outputSource);
		context->outputSource = source;
	} else {
		obs_source_release(source);
	}
	context->width = obs_source_get_width(context->outputSource);
	context->height = obs_source_get_height(context->outputSource);
	if (context->recurring) {
		obs_enter_graphics();
		if (!context->render) {

			context->render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		} else {
			gs_texrender_reset(context->render);
		}
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		if (gs_texrender_begin(context->render, context->width, context->height)) {
			struct vec4 clear_color;

			vec4_zero(&clear_color);
			gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
			gs_ortho(0.0f, (float)context->width, 0.0f, (float)context->height, -100.0f, 100.0f);

			obs_source_video_render(context->outputSource);

			gs_texrender_end(context->render);
		}
		gs_blend_state_pop();
		obs_leave_graphics();
	}
}

struct obs_source_info output_source_info = {
	.id = "kplive_ads_program_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = output_source_get_name,
	.create = output_source_create,
	.destroy = output_source_destroy,
	.load = output_source_update,
	.update = output_source_update,
	.get_properties = output_source_properties,
	.get_defaults = output_source_defaults,
	.video_render = output_source_video_render,
	.video_tick = output_source_video_tick,
	.get_width = output_source_getwidth,
	.get_height = output_source_getheight,
	.icon_type = OBS_ICON_TYPE_UNKNOWN,
};
