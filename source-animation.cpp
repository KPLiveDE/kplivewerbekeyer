#include "downstream-keyer.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <obs-frontend-api.h>
#include <obs-module.h>

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

namespace {
struct FindSceneItemData {
	const char *sourceName = nullptr;
	obs_sceneitem_t *item = nullptr;
};

bool find_scene_item_cb(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *data = static_cast<FindSceneItemData *>(param);
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = source ? obs_source_get_name(source) : nullptr;
	if (name && data->sourceName && strcmp(name, data->sourceName) == 0) {
		obs_sceneitem_addref(item);
		data->item = item;
		return false;
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *group = obs_sceneitem_group_get_scene(item);
		if (group)
			obs_scene_enum_items(group, find_scene_item_cb, data);
		if (data->item)
			return false;
	}
	return true;
}

obs_sceneitem_t *find_scene_item(obs_source_t *sceneSource, const char *sourceName)
{
	if (!sceneSource || !sourceName || !*sourceName)
		return nullptr;
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (!scene)
		return nullptr;
	FindSceneItemData data{sourceName, nullptr};
	obs_scene_enum_items(scene, find_scene_item_cb, &data);
	return data.item;
}

struct CollectSceneItemsData {
	std::vector<std::string> names;
};

bool collect_scene_items_cb(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *data = static_cast<CollectSceneItemsData *>(param);
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = source ? obs_source_get_name(source) : nullptr;
	if (name && *name && std::find(data->names.begin(), data->names.end(), name) == data->names.end())
		data->names.emplace_back(name);

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *group = obs_sceneitem_group_get_scene(item);
		if (group)
			obs_scene_enum_items(group, collect_scene_items_cb, data);
	}
	return true;
}

std::vector<std::string> collect_scene_items(obs_source_t *sceneSource)
{
	CollectSceneItemsData data;
	obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource) : nullptr;
	if (scene)
		obs_scene_enum_items(scene, collect_scene_items_cb, &data);
	return data.names;
}

void add_preset_items(QComboBox *combo)
{
	combo->addItem(QT_UTF8(obs_module_text("AnimationNone")), (int)SourceAnimationPreset::none);
	combo->addItem(QT_UTF8(obs_module_text("AnimationFromLeft")), (int)SourceAnimationPreset::fromLeft);
	combo->addItem(QT_UTF8(obs_module_text("AnimationFromRight")), (int)SourceAnimationPreset::fromRight);
	combo->addItem(QT_UTF8(obs_module_text("AnimationFromTop")), (int)SourceAnimationPreset::fromTop);
	combo->addItem(QT_UTF8(obs_module_text("AnimationFromBottom")), (int)SourceAnimationPreset::fromBottom);
	combo->addItem(QT_UTF8(obs_module_text("AnimationZoomIn")), (int)SourceAnimationPreset::zoomIn);
	combo->addItem(QT_UTF8(obs_module_text("AnimationZoomOut")), (int)SourceAnimationPreset::zoomOut);
	combo->addItem(QT_UTF8(obs_module_text("AnimationSpinLeft")), (int)SourceAnimationPreset::spinLeft);
	combo->addItem(QT_UTF8(obs_module_text("AnimationSpinRight")), (int)SourceAnimationPreset::spinRight);
}

void add_easing_items(QComboBox *combo)
{
	combo->addItem(QT_UTF8(obs_module_text("AnimationLinear")), (int)SourceAnimationEasing::linear);
	combo->addItem(QT_UTF8(obs_module_text("AnimationEaseIn")), (int)SourceAnimationEasing::easeIn);
	combo->addItem(QT_UTF8(obs_module_text("AnimationEaseOut")), (int)SourceAnimationEasing::easeOut);
	combo->addItem(QT_UTF8(obs_module_text("AnimationEaseInOut")), (int)SourceAnimationEasing::easeInOutCubic);
}

void select_data(QComboBox *combo, int value)
{
	const int index = combo->findData(value);
	if (index >= 0)
		combo->setCurrentIndex(index);
}
} // namespace

SourceAnimationTransform DownstreamKeyer::get_sceneitem_transform(obs_sceneitem_t *item)
{
	SourceAnimationTransform transform;
	if (!item)
		return transform;
	obs_sceneitem_get_pos(item, &transform.pos);
	obs_sceneitem_get_scale(item, &transform.scale);
	obs_sceneitem_get_bounds(item, &transform.bounds);
	transform.rot = obs_sceneitem_get_rot(item);
	transform.useBounds = obs_sceneitem_get_bounds_type(item) != OBS_BOUNDS_NONE;
	return transform;
}

void DownstreamKeyer::set_sceneitem_transform(obs_sceneitem_t *item, const SourceAnimationTransform &transform)
{
	if (!item)
		return;
	obs_sceneitem_defer_update_begin(item);
	obs_sceneitem_set_pos(item, &transform.pos);
	obs_sceneitem_set_scale(item, &transform.scale);
	obs_sceneitem_set_bounds(item, &transform.bounds);
	obs_sceneitem_set_rot(item, transform.rot);
	obs_sceneitem_defer_update_end(item);
}

SourceAnimationTransform DownstreamKeyer::preset_transform(const SourceAnimationTransform &base, SourceAnimationPreset preset,
						     uint32_t canvasWidth, uint32_t canvasHeight)
{
	SourceAnimationTransform result = base;
	switch (preset) {
	case SourceAnimationPreset::fromLeft:
		result.pos.x -= (float)canvasWidth;
		break;
	case SourceAnimationPreset::fromRight:
		result.pos.x += (float)canvasWidth;
		break;
	case SourceAnimationPreset::fromTop:
		result.pos.y -= (float)canvasHeight;
		break;
	case SourceAnimationPreset::fromBottom:
		result.pos.y += (float)canvasHeight;
		break;
	case SourceAnimationPreset::zoomIn:
		if (result.useBounds) {
			result.bounds.x *= 0.05f;
			result.bounds.y *= 0.05f;
		} else {
			result.scale.x *= 0.05f;
			result.scale.y *= 0.05f;
		}
		break;
	case SourceAnimationPreset::zoomOut:
		if (result.useBounds) {
			result.bounds.x *= 1.75f;
			result.bounds.y *= 1.75f;
		} else {
			result.scale.x *= 1.75f;
			result.scale.y *= 1.75f;
		}
		break;
	case SourceAnimationPreset::spinLeft:
		if (result.useBounds) {
			result.bounds.x *= 0.10f;
			result.bounds.y *= 0.10f;
		} else {
			result.scale.x *= 0.10f;
			result.scale.y *= 0.10f;
		}
		result.rot -= 180.0f;
		break;
	case SourceAnimationPreset::spinRight:
		if (result.useBounds) {
			result.bounds.x *= 0.10f;
			result.bounds.y *= 0.10f;
		} else {
			result.scale.x *= 0.10f;
			result.scale.y *= 0.10f;
		}
		result.rot += 180.0f;
		break;
	case SourceAnimationPreset::none:
	default:
		break;
	}
	return result;
}

float DownstreamKeyer::easing_value(SourceAnimationEasing easing, float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	switch (easing) {
	case SourceAnimationEasing::easeIn:
		return t * t * t;
	case SourceAnimationEasing::easeOut: {
		const float p = 1.0f - t;
		return 1.0f - p * p * p;
	}
	case SourceAnimationEasing::easeInOutCubic:
		return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
	case SourceAnimationEasing::linear:
	default:
		return t;
	}
}

void DownstreamKeyer::cancel_source_animation(bool restore)
{
	sourceAnimationTimer.stop();
	if (activeSourceAnimation.item) {
		if (restore)
			set_sceneitem_transform(activeSourceAnimation.item, activeSourceAnimation.restore);
		obs_sceneitem_release(activeSourceAnimation.item);
	}
	activeSourceAnimation = {};
	sourceAnimationRunning = false;
	exitAnimationInProgress = false;
}

bool DownstreamKeyer::start_source_animation(obs_source_t *sceneSource, bool hide)
{
	if (!sceneSource)
		return false;
	const char *sceneName = obs_source_get_name(sceneSource);
	if (!sceneName)
		return false;
	const auto configIt = sourceAnimations.find(sceneName);
	if (configIt == sourceAnimations.end() || !configIt->second.enabled || configIt->second.sourceName.empty())
		return false;

	const auto &config = configIt->second;
	const SourceAnimationPreset preset = hide ? config.hidePreset : config.showPreset;
	const int duration = hide ? config.hideDuration : config.showDuration;
	if (preset == SourceAnimationPreset::none || duration <= 0)
		return false;

	obs_sceneitem_t *item = find_scene_item(sceneSource, config.sourceName.c_str());
	if (!item) {
		blog(LOG_WARNING, "[KPLive Ads Keyer] animation source '%s' not found in scene '%s'", config.sourceName.c_str(), sceneName);
		return false;
	}

	cancel_source_animation(true);
	const SourceAnimationTransform base = get_sceneitem_transform(item);
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	obs_source_t *sceneObsSource = scene ? obs_scene_get_source(scene) : sceneSource;
	uint32_t canvasWidth = sceneObsSource ? obs_source_get_width(sceneObsSource) : 1920;
	uint32_t canvasHeight = sceneObsSource ? obs_source_get_height(sceneObsSource) : 1080;
	if (!canvasWidth)
		canvasWidth = 1920;
	if (!canvasHeight)
		canvasHeight = 1080;

	activeSourceAnimation.item = item;
	activeSourceAnimation.restore = base;
	activeSourceAnimation.duration = duration;
	activeSourceAnimation.easing = config.easing;
	activeSourceAnimation.hide = hide;
	if (hide) {
		activeSourceAnimation.from = base;
		activeSourceAnimation.to = preset_transform(base, preset, canvasWidth, canvasHeight);
		exitAnimationInProgress = true;
	} else {
		activeSourceAnimation.from = preset_transform(base, preset, canvasWidth, canvasHeight);
		activeSourceAnimation.to = base;
		set_sceneitem_transform(item, activeSourceAnimation.from);
	}

	sourceAnimationRunning = true;
	sourceAnimationClock.restart();
	sourceAnimationTimer.start();
	blog(LOG_INFO, "[KPLive Ads Keyer] %s animation started for '%s' in scene '%s' (%d ms)", hide ? "hide" : "show",
	     config.sourceName.c_str(), sceneName, duration);
	return true;
}

void DownstreamKeyer::source_animation_tick()
{
	if (!sourceAnimationRunning || !activeSourceAnimation.item) {
		sourceAnimationTimer.stop();
		return;
	}

	const float raw = activeSourceAnimation.duration > 0
				  ? (float)sourceAnimationClock.elapsed() / (float)activeSourceAnimation.duration
				  : 1.0f;
	const float t = easing_value(activeSourceAnimation.easing, raw);
	SourceAnimationTransform current;
	current.useBounds = activeSourceAnimation.from.useBounds;
	current.pos.x = activeSourceAnimation.from.pos.x + (activeSourceAnimation.to.pos.x - activeSourceAnimation.from.pos.x) * t;
	current.pos.y = activeSourceAnimation.from.pos.y + (activeSourceAnimation.to.pos.y - activeSourceAnimation.from.pos.y) * t;
	current.scale.x =
		activeSourceAnimation.from.scale.x + (activeSourceAnimation.to.scale.x - activeSourceAnimation.from.scale.x) * t;
	current.scale.y =
		activeSourceAnimation.from.scale.y + (activeSourceAnimation.to.scale.y - activeSourceAnimation.from.scale.y) * t;
	current.bounds.x =
		activeSourceAnimation.from.bounds.x + (activeSourceAnimation.to.bounds.x - activeSourceAnimation.from.bounds.x) * t;
	current.bounds.y =
		activeSourceAnimation.from.bounds.y + (activeSourceAnimation.to.bounds.y - activeSourceAnimation.from.bounds.y) * t;
	current.rot = activeSourceAnimation.from.rot + (activeSourceAnimation.to.rot - activeSourceAnimation.from.rot) * t;
	set_sceneitem_transform(activeSourceAnimation.item, current);

	if (raw < 1.0f)
		return;

	const bool wasHide = activeSourceAnimation.hide;
	obs_sceneitem_t *item = activeSourceAnimation.item;
	const SourceAnimationTransform restore = activeSourceAnimation.restore;
	if (!wasHide)
		set_sceneitem_transform(item, activeSourceAnimation.to);
	activeSourceAnimation = {};
	sourceAnimationRunning = false;
	sourceAnimationTimer.stop();

	if (wasHide) {
		exitAnimationInProgress = false;
		apply_source_immediate(nullptr, true);
		set_sceneitem_transform(item, restore);
	}
	obs_sceneitem_release(item);
}

void DownstreamKeyer::ConfigureSourceAnimation(QWidget *parent)
{
	QListWidgetItem *sceneItem = scenesList->currentItem();
	if (!sceneItem || !sceneItem->isSelected()) {
		QMessageBox::information(parent ? parent : this, QT_UTF8(obs_module_text("AnimationTitle")),
					 QT_UTF8(obs_module_text("AnimationSelectScene")));
		return;
	}

	const QString sceneName = sceneItem->text();
	obs_source_t *sceneSource = canvas ? obs_canvas_get_source_by_name(canvas, QT_TO_UTF8(sceneName))
					   : obs_get_source_by_name(QT_TO_UTF8(sceneName));
	if (!sceneSource || !obs_source_is_scene(sceneSource)) {
		obs_source_release(sceneSource);
		QMessageBox::warning(parent ? parent : this, QT_UTF8(obs_module_text("AnimationTitle")),
				       QT_UTF8(obs_module_text("AnimationSceneUnavailable")));
		return;
	}

	const auto sourceNames = collect_scene_items(sceneSource);
	if (sourceNames.empty()) {
		obs_source_release(sceneSource);
		QMessageBox::information(parent ? parent : this, QT_UTF8(obs_module_text("AnimationTitle")),
					 QT_UTF8(obs_module_text("AnimationNoSources")));
		return;
	}

	SourceAnimationConfig config;
	const auto it = sourceAnimations.find(QT_TO_UTF8(sceneName));
	if (it != sourceAnimations.end())
		config = it->second;
	else
		config.enabled = true;

	QDialog dialog(parent ? parent : this);
	dialog.setWindowTitle(QT_UTF8(obs_module_text("AnimationTitle")) + " – " + sceneName);
	auto *layout = new QVBoxLayout(&dialog);
	auto *enabled = new QCheckBox(QT_UTF8(obs_module_text("AnimationEnabled")), &dialog);
	enabled->setChecked(config.enabled);
	layout->addWidget(enabled);

	auto *form = new QFormLayout;
	auto *sourceCombo = new QComboBox(&dialog);
	for (const auto &sourceName : sourceNames)
		sourceCombo->addItem(QT_UTF8(sourceName.c_str()));
	if (!config.sourceName.empty()) {
		const int sourceIndex = sourceCombo->findText(QT_UTF8(config.sourceName.c_str()));
		if (sourceIndex >= 0)
			sourceCombo->setCurrentIndex(sourceIndex);
	}
	form->addRow(QT_UTF8(obs_module_text("AnimationSource")), sourceCombo);

	auto *showPreset = new QComboBox(&dialog);
	add_preset_items(showPreset);
	select_data(showPreset, (int)config.showPreset);
	form->addRow(QT_UTF8(obs_module_text("AnimationShowPreset")), showPreset);

	auto *showDuration = new QSpinBox(&dialog);
	showDuration->setRange(50, 10000);
	showDuration->setSingleStep(50);
	showDuration->setSuffix(" ms");
	showDuration->setValue(config.showDuration);
	form->addRow(QT_UTF8(obs_module_text("AnimationShowDuration")), showDuration);

	auto *hidePreset = new QComboBox(&dialog);
	add_preset_items(hidePreset);
	select_data(hidePreset, (int)config.hidePreset);
	form->addRow(QT_UTF8(obs_module_text("AnimationHidePreset")), hidePreset);

	auto *hideDuration = new QSpinBox(&dialog);
	hideDuration->setRange(50, 10000);
	hideDuration->setSingleStep(50);
	hideDuration->setSuffix(" ms");
	hideDuration->setValue(config.hideDuration);
	form->addRow(QT_UTF8(obs_module_text("AnimationHideDuration")), hideDuration);

	auto *easing = new QComboBox(&dialog);
	add_easing_items(easing);
	select_data(easing, (int)config.easing);
	form->addRow(QT_UTF8(obs_module_text("AnimationEasing")), easing);
	layout->addLayout(form);

	auto *hint = new QLabel(QT_UTF8(obs_module_text("AnimationHint")), &dialog);
	hint->setWordWrap(true);
	layout->addWidget(hint);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	if (dialog.exec() == QDialog::Accepted) {
		config.enabled = enabled->isChecked();
		config.sourceName = QT_TO_UTF8(sourceCombo->currentText());
		config.showPreset = static_cast<SourceAnimationPreset>(showPreset->currentData().toInt());
		config.hidePreset = static_cast<SourceAnimationPreset>(hidePreset->currentData().toInt());
		config.showDuration = showDuration->value();
		config.hideDuration = hideDuration->value();
		config.easing = static_cast<SourceAnimationEasing>(easing->currentData().toInt());
		sourceAnimations[QT_TO_UTF8(sceneName)] = config;
		obs_frontend_save();
	}

	obs_source_release(sceneSource);
}
