/*
   Copyright (C) 2011, 2015 by Ignacio R. Morelle <shadowm2006@gmail.com>
   Copyright (C) 2016 by Charles Dang <exodia339gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/preferences_dialog.hpp"

#include "game_preferences.hpp"
#include "preferences.hpp"
#include "preferences_display.hpp"
#include "lobby_preferences.hpp"
#include "gettext.hpp"
#include "video.hpp"

#include "gui/auxiliary/find_widget.tpp"

// Sub-dialog includes
#include "gui/dialogs/advanced_graphics_options.hpp"
#include "gui/dialogs/game_cache_options.hpp"
#include "gui/dialogs/mp_alerts_options.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/combobox.hpp"
#include "gui/widgets/grid.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/stacked_widget.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "util.hpp"
#include "utils/foreach.tpp"

#include "gettext.hpp"

#include <sstream>
#include <boost/bind.hpp>
#include <boost/math/common_factor_rt.hpp>

namespace {

struct advanced_preferences_sorter
{
	bool operator()(const config& lhs, const config& rhs) const
	{
			return lhs["name"].t_str().str() < rhs["name"].t_str().str();
	}
};

const std::string bool_to_display_string(bool value)
{
	return value ? _("yes") : _("no");
}

} // end anon namespace


namespace gui2 {

// TODO: probably should use a namespace alias instead
using namespace preferences;

REGISTER_DIALOG(preferences)

tpreferences::tpreferences(CVideo& video, const config& game_cfg)
	: resolutions_(video.get_available_resolutions(true))
	, adv_preferences_cfg_()
	, friend_names_()
	, last_selected_item_(0)
	, accl_speeds_()
{
	BOOST_FOREACH(const config& adv, game_cfg.child_range("advanced_preference")) {
		adv_preferences_cfg_.push_back(adv);
	}

	std::sort(adv_preferences_cfg_.begin(), adv_preferences_cfg_.end(), 
		advanced_preferences_sorter());

	// IMPORTANT: NEVER have trailing zeroes here, or else the cast from doubles
	// to string will not match, since lexical_cast strips trailing zeroes.
	static const char* const speeds[] = {
		"0.25", "0.5", "0.75", "1", "1.25", "1.5", "1.75", "2", "3", "4", "8", "16" };

	const size_t num_items = sizeof(speeds)/sizeof(const char*);
	accl_speeds_.insert(accl_speeds_.end(), speeds, &speeds[num_items]);
}

// Determine the template type in order to use the correct value getter
static std::string disambiguate_widget_value(const ttoggle_button& parent_widget)
{
	return bool_to_display_string(parent_widget.get_value_bool());
}

static std::string disambiguate_widget_value(const tcombobox& parent_widget)
{
	return parent_widget.get_value_string();
}

static std::string disambiguate_widget_value(const tslider& parent_widget)
{
	return parent_widget.get_value_label();
}

// Parse the max atosaves slider value. Max value means infinte autosaves.
static std::string get_max_autosaves_status_label(const tslider& slider)
{
	std::string label;
	const int value = slider.get_value();

	// INFINITE_AUTO_SAVES is hardcoded as 61 in game_preferences.hpp
	if(value == INFINITE_AUTO_SAVES) {
		// label = _("∞"); Doesn't look good on Windows. Restore when it does
		label = _("infinite");
	} else {
		label = disambiguate_widget_value(slider);
	}

	return label;
}

// Helper function to refresh resolution list
static void set_resolution_list(tcombobox& res_list, CVideo& video)
{
	const std::vector<std::pair<int,int> > resolutions = video.get_available_resolutions(true);

	std::vector<std::string> options;
	FOREACH(const AUTO& res, resolutions)
	{
		std::ostringstream option;
		option << res.first << utils::unicode_multiplication_sign << res.second;

		const int div = boost::math::gcd(res.first, res.second);
		const int ratio[2] = {res.first/div, res.second/div};
		if (ratio[0] <= 10 || ratio[1] <= 10) {
			option << " <span color='#777777'>(" 
				<< ratio[0] << ':' << ratio[1] << ")</span>";
		}

		options.push_back(option.str());
	}

	const unsigned current_res = std::find(resolutions.begin(), resolutions.end(), 
		video.current_resolution()) - resolutions.begin();

	res_list.set_values(options, current_res);
}

void tpreferences::setup_single_toggle(
		const std::string& widget_id,
		const bool start_value,
		boost::function<void(bool)> callback,
		twidget& find_in)
{
	ttoggle_button& widget =
		find_widget<ttoggle_button>(&find_in, widget_id, false);

	widget.set_value(start_value);

	connect_signal_mouse_left_click(widget, boost::bind(
		&tpreferences::single_toggle_callback,
		this, boost::ref(widget), callback));
}

void tpreferences::setup_toggle_slider_pair(
		const std::string& toggle_widget,
		const std::string& slider_widget,
		const bool toggle_start_value,
		const int slider_state_value,
		boost::function<void(bool)> toggle_callback,
		boost::function<void(int)> slider_callback,
		twidget& find_in)
{
	ttoggle_button& button =
		find_widget<ttoggle_button>(&find_in, toggle_widget, false);
	tslider& slider =
		find_widget<tslider>(&find_in, slider_widget, false);

	button.set_value(toggle_start_value);
	slider.set_value(slider_state_value);
	slider.set_active(toggle_start_value);

	connect_signal_mouse_left_click(button, boost::bind(
		&tpreferences::toggle_slider_pair_callback,
		this, boost::ref(button), boost::ref(slider),
		toggle_callback));

	connect_signal_notify_modified(slider, boost::bind(
		&tpreferences::single_slider_callback,
		this, boost::ref(slider),
		slider_callback));
}

void tpreferences::setup_single_slider(
		const std::string& widget_id,
		const int start_value,
		boost::function<void(int)> slider_callback,
		twidget& find_in)
{
	tslider& widget = find_widget<tslider>(&find_in, widget_id, false);
	widget.set_value(start_value);

	connect_signal_notify_modified(widget, boost::bind(
		&tpreferences::single_slider_callback,
		this, boost::ref(widget),
		slider_callback));
}

void tpreferences::setup_combobox(
		const std::string& widget_id,
		const combo_data& options,
		const unsigned start_value,
		boost::function<void(std::string)> callback,
		twidget& find_in)
{
	tcombobox& widget =
		find_widget<tcombobox>(&find_in, widget_id, false);

	widget.set_use_markup(true);
	widget.set_values(options.first, start_value);

	connect_signal_mouse_left_click(widget, boost::bind(
		&tpreferences::simple_combobox_callback,
		this, boost::ref(widget),
		callback, options.second));
}

void tpreferences::setup_radio_toggle(
		const std::string& toggle_id,
		LOBBY_JOINS enum_value,
		int start_value,
		std::vector<std::pair<ttoggle_button*, int> >& vec,
		twindow& window)
{
	ttoggle_button& button = find_widget<ttoggle_button>(&window, toggle_id, false);

	button.set_value(enum_value == start_value);

	connect_signal_mouse_left_click(button, boost::bind(
		&tpreferences::toggle_radio_callback,
		this, boost::ref(vec), boost::ref(start_value), &button));

	vec.push_back(std::make_pair(&button, enum_value));
}

template <typename T>
void tpreferences::bind_status_label(T& parent, const std::string& label_id,
		twidget& find_in)
{
	tcontrol& label = find_widget<tcontrol>(&find_in, label_id, false);
	label.set_label(disambiguate_widget_value(parent));

	parent.set_callback_state_change(boost::bind(
		&tpreferences::status_label_callback<T>,
		this, boost::ref(parent), boost::ref(label)));
}

void tpreferences::bind_status_label(tslider& parent, const std::string& label_id,
		twidget& find_in)
{
	tcontrol& label = find_widget<tcontrol>(&find_in, label_id, false);
	label.set_label(lexical_cast<std::string>(parent.get_value_label()));

	connect_signal_notify_modified(parent, boost::bind(
		&tpreferences::status_label_callback<tslider>,
		this, boost::ref(parent), boost::ref(label)));
}

void tpreferences::setup_friends_list(twindow& window)
{
	tlistbox& friends_list = find_widget<tlistbox>(&window, "friends_list", false);

	const std::map<std::string, preferences::acquaintance>& acquaintances = get_acquaintances();

	std::map<std::string, string_map> data;

	find_widget<tbutton>(&window, "remove", false).set_active(!acquaintances.empty());

	friends_list.clear();
	friend_names_.clear();

	if (acquaintances.empty()) {
		data["friend_icon"]["label"] = "misc/status-neutral.png";
		data["friend_name"]["label"] = _("Empty list");
		friends_list.add_row(data);

		return;
	}

	FOREACH(const AUTO& acquaintence, acquaintances)
	{
		std::string image = "friend.png";
		std::string descriptor = _("friend");
		std::string notes;

		if(acquaintence.second.get_status() == "ignore") {
			image = "ignore.png";
			descriptor = _("ignored");
		}

		if(!acquaintence.second.get_notes().empty()) {
			notes = " <small>(" + acquaintence.second.get_notes() + ")</small>";
		}

		data["friend_icon"]["label"] = "misc/status-" + image;

		data["friend_name"]["label"] = acquaintence.second.get_nick() + notes;
		data["friend_name"]["use_markup"] = "true";

		data["friend_status"]["label"] = "<small>" + descriptor + "</small>";
		data["friend_status"]["use_markup"] = "true";
		friends_list.add_row(data);

		friend_names_.push_back(acquaintence.first);
	}
}

void tpreferences::add_friend_list_entry(const bool is_friend,
		ttext_box& textbox, twindow& window)
{
	std::string reason;
	std::string username = textbox.text();
	size_t pos = username.find_first_of(' ');

	if (pos != std::string::npos) {
		reason = username.substr(pos + 1);
		username = username.substr(0, pos);
	}

	const bool added_sucessfully = is_friend ? 
		add_friend(username, reason) : 
		add_ignore(username, reason) ;

	if (!added_sucessfully) {
		gui2::show_transient_error_message(window.video(), _("Invalid username"));
		return;
	}

	textbox.clear();
	setup_friends_list(window);
}

void tpreferences::remove_friend_list_entry(tlistbox& friends_list, 
		ttext_box& textbox, twindow& window)
{
	std::string to_remove = textbox.text();

	if (to_remove.empty()) {
		const int selected_row = std::max(0, friends_list.get_selected_row());
		to_remove = friend_names_[selected_row];
	}

	if(!remove_acquaintance(to_remove)) {
		gui2::show_transient_error_message(window.video(), _("Not on friends or ignore lists"));
		return;
	}

	textbox.clear();
	setup_friends_list(window);
}

// Helper function to get the main grid in each row of the advanced section
// lisbox, which contains the value and setter widgets.
static tgrid* get_advanced_row_grid(tlistbox& list, const int selected_row)
{
	return dynamic_cast<tgrid*>(
		list.get_row_grid(selected_row)->find("pref_main_grid", false));
}

/**
 * Sets up states and callbacks for each of the widgets
 */
void tpreferences::initialize_members(twindow& window)
{
	//
	// GENERAL PANEL
	// 

	/** SCROLL SPEED **/
	setup_single_slider("scroll_speed",
		scroll_speed(), set_scroll_speed, window);

	/** ACCELERATED SPEED **/
	ttoggle_button& accl_toggle =
		find_widget<ttoggle_button>(&window, "turbo_toggle", false);
	tslider& accl_slider =
		find_widget<tslider>(&window, "turbo_slider", false);

	const int selected_speed = std::find(
		  (accl_speeds_.begin()), accl_speeds_.end(), lexical_cast<std::string>(turbo_speed())) 
		- (accl_speeds_.begin());

	accl_slider.set_value_labels(accl_speeds_);

	const bool is_turbo = turbo();

	accl_toggle.set_value(is_turbo);
	accl_slider.set_value(selected_speed + 1);
	accl_slider.set_active(is_turbo);

	connect_signal_mouse_left_click(accl_toggle, boost::bind(
		&tpreferences::toggle_slider_pair_callback,
		this, boost::ref(accl_toggle), boost::ref(accl_slider),
		set_turbo));

	connect_signal_notify_modified(accl_slider, boost::bind(
		&tpreferences::accl_speed_slider_callback,
		this,
		boost::ref(accl_slider)));

	bind_status_label(accl_slider, "turbo_value", window);

	/** SKIP AI MOVES **/
	setup_single_toggle("skip_ai_moves",
		show_ai_moves(), set_show_ai_moves, window);

	/** DISABLE AUTO MOVES **/
	setup_single_toggle("disable_auto_moves",
		disable_auto_moves(), set_disable_auto_moves, window);

	/** TURN DIALOG **/
	setup_single_toggle("show_turn_dialog",
		turn_dialog(), set_turn_dialog, window);

	/** ENABLE PLANNING MODE **/
	setup_single_toggle("whiteboard_on_start",
		enable_whiteboard_mode_on_start(), set_enable_whiteboard_mode_on_start, window);

	/** HIDE ALLY PLANS **/
	setup_single_toggle("whiteboard_hide_allies",
		hide_whiteboard(), set_hide_whiteboard, window);

	/** INTERRUPT ON SIGHTING **/
	setup_single_toggle("interrupt_move_when_ally_sighted",
		interrupt_when_ally_sighted(), set_interrupt_when_ally_sighted, window);

	/** SAVE REPLAYS **/
	setup_single_toggle("save_replays",
		save_replays(), set_save_replays, window);

	/** DELETE AUTOSAVES **/
	setup_single_toggle("delete_saves",
		delete_saves(), set_delete_saves, window);

	/** MAX AUTO SAVES **/
	setup_single_slider("max_saves_slider",
		autosavemax(), set_autosavemax, window);

	tslider& autosaves_slider = find_widget<tslider>(&window, "max_saves_slider", false);
	tcontrol& autosaves_label = find_widget<tcontrol>(&window, "max_saves_value", false);

	autosaves_label.set_label(get_max_autosaves_status_label(autosaves_slider));
	autosaves_label.set_use_markup(true);

	connect_signal_notify_modified(autosaves_slider, boost::bind(
		&tpreferences::max_autosaves_slider_callback,
		this, boost::ref(autosaves_slider), boost::ref(autosaves_label)));

	/** SET HOTKEYS **/
	connect_signal_mouse_left_click(find_widget<tbutton>(&window, "hotkeys", false),
			boost::bind(&preferences::show_hotkeys_preferences_dialog,
			boost::ref(window.video())));

	/** CACHE MANAGE **/
	connect_signal_mouse_left_click(find_widget<tbutton>(&window, "cachemg", false),
			boost::bind(&gui2::tgame_cache_options::display,
			boost::ref(window.video())));

	//
	// DISPLAY PANEL
	//

	/** FULLSCREEN TOGGLE **/
	ttoggle_button& toggle_fullscreen =
			find_widget<ttoggle_button>(&window, "fullscreen", false);

	toggle_fullscreen.set_value(fullscreen());

	// We bind a special callback function, so setup_single_toggle() is not used
	connect_signal_mouse_left_click(toggle_fullscreen, boost::bind(
			&tpreferences::fullscreen_toggle_callback,
			this, boost::ref(window)));

	/** SET RESOLUTION **/
	tcombobox& res_list = find_widget<tcombobox>(&window, "resolution_set", false);

	res_list.set_use_markup(true);
	res_list.set_active(!fullscreen());
	set_resolution_list(res_list, window.video());

	res_list.connect_click_handler(
			boost::bind(&tpreferences::handle_res_select,
			this, boost::ref(window)));

	/** SHOW FLOATING LABELS **/
	setup_single_toggle("show_floating_labels",
		show_floating_labels(), set_show_floating_labels, window);

	/** SHOW HALOES **/
	setup_single_toggle("show_halos",
		show_haloes(), set_show_haloes, window);

	/** SHOW TEAM COLORS **/
	setup_single_toggle("show_ellipses",
		show_side_colors(), set_show_side_colors, window);

	/** SHOW GRID **/
	setup_single_toggle("show_grid",
		grid(), set_grid, window);

	/** ANIMATE MAP **/
	setup_single_toggle("animate_terrains",
		animate_map(), set_animate_map, window);

	/** SHOW UNIT STANDING ANIMS **/
	setup_single_toggle("animate_units_standing",
		show_standing_animations(), set_show_standing_animations, window);

	/** SHOW UNIT IDLE ANIMS **/
	setup_toggle_slider_pair("animate_units_idle", "idle_anim_frequency",
		idle_anim(), idle_anim_rate(),
		set_idle_anim, set_idle_anim_rate, window);

	/** SELECT THEME **/
	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "choose_theme", false),
			boost::bind(&preferences::show_theme_dialog,
			boost::ref(window.video())));


	//
	// SOUND PANEL
	//

	/** SOUND FX **/
	setup_toggle_slider_pair("sound_toggle_sfx", "sound_volume_sfx",
		sound_on(), sound_volume(),
		set_sound, set_sound_volume, window);

	/** MUSIC **/
	setup_toggle_slider_pair("sound_toggle_music", "sound_volume_music",
		music_on(), music_volume(),
		set_music, set_music_volume, window);

	/** TURN BELL **/
	setup_toggle_slider_pair("sound_toggle_bell", "sound_volume_bell",
		turn_bell(), bell_volume(),
		set_turn_bell, set_bell_volume, window);

	/** UI FX **/
	setup_toggle_slider_pair("sound_toggle_uisfx", "sound_volume_uisfx",
		UI_sound_on(), UI_volume(),
		set_UI_sound, set_UI_volume, window);


	//
	// MULTIPLAYER PANEL
	//

	/** CHAT LINES **/
	setup_single_slider("chat_lines",
		chat_lines(), set_chat_lines, window);

	/** CHAT TIMESTAMPPING **/
	setup_single_toggle("chat_timestamps",
		chat_timestamping(), set_chat_timestamping, window);

	/** SAVE PASSWORD **/
	setup_single_toggle("remember_password",
		remember_password(), set_remember_password, window);

	/** SORT LOBBY LIST **/
	setup_single_toggle("lobby_sort_players",
		sort_list(), _set_sort_list, window);

	/** ICONIZE LOBBY LIST **/
	setup_single_toggle("lobby_player_icons",
		iconize_list(), _set_iconize_list, window);

	/** WHISPERS FROM FRIENDS ONLY **/
	setup_single_toggle("lobby_whisper_friends_only",
		whisper_friends_only(), set_whisper_friends_only, window);

	/** LOBBY JOIN NOTIFICATIONS **/
	setup_radio_toggle("lobby_joins_none", SHOW_NONE,
		lobby_joins(), lobby_joins_, window);
	setup_radio_toggle("lobby_joins_friends", SHOW_FRIENDS,
		lobby_joins(), lobby_joins_, window);
	setup_radio_toggle("lobby_joins_all", SHOW_ALL,
		lobby_joins(), lobby_joins_, window);

	/** FRIENDS LIST **/
	setup_friends_list(window);

	ttext_box& textbox = find_widget<ttext_box>(&window, "friend_name_box", false);
	tlistbox& friend_list = find_widget<tlistbox>(&window, "friends_list", false);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "add_friend", false), boost::bind(
			&tpreferences::add_friend_list_entry,
			this, true,
			boost::ref(textbox),
			boost::ref(window)));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "add_ignored", false), boost::bind(
			&tpreferences::add_friend_list_entry,
			this, false,
			boost::ref(textbox),
			boost::ref(window)));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "remove", false), boost::bind(
			&tpreferences::remove_friend_list_entry,
			this,
			boost::ref(friend_list),
			boost::ref(textbox),
			boost::ref(window)));

	friend_list.select_row(0);

	/** ALERTS **/
	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "mp_alerts", false),
			boost::bind(&gui2::tmp_alerts_options::display,
			boost::ref(window.video())));

	/** SET WESNOTHD PATH **/
	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "mp_wesnothd", false), boost::bind(
			&preferences::show_wesnothd_server_search,
			boost::ref(window.video())));


	//
	// ADVANCED PANEL
	//

	tlistbox& advanced = find_widget<tlistbox>(&window, "advanced_prefs", false);

	std::map<std::string, string_map> row_data;

	BOOST_FOREACH(const config& option, adv_preferences_cfg_)
	{
		// Details about the current option
		const ADVANCED_PREF_TYPE& pref_type = ADVANCED_PREF_TYPE::string_to_enum(
			option["type"].str());
		const std::string& pref_name = option["field"].str();

		row_data["pref_name"]["label"] = option["name"];
		advanced.add_row(row_data);

		const int this_row = advanced.get_item_count() - 1;

		// Get the main grid from each row
		tgrid* main_grid = get_advanced_row_grid(advanced, this_row);
		assert(main_grid);

		tgrid* details_grid = &find_widget<tgrid>(main_grid, "prefs_setter_grid", false);
		details_grid->set_visible(tcontrol::tvisible::invisible);

		// The toggle widget for toggle-type options (hidden for other types)
		ttoggle_button& toggle_box = find_widget<ttoggle_button>(main_grid, "value_toggle", false);
		toggle_box.set_visible(tcontrol::tvisible::hidden);

		if(!option["description"].empty()) {
			find_widget<tcontrol>(main_grid, "description", false).set_label(option["description"]);
		}

		switch (pref_type.v) {
			case ADVANCED_PREF_TYPE::TOGGLE: {
				//main_grid->remove_child("setter");

				toggle_box.set_visible(tcontrol::tvisible::visible);

				// Needed to disambiguate overloaded function
				typedef void (*setter) (const std::string &, bool);
				setter set_ptr = &preferences::set;

				setup_single_toggle("value_toggle",
					get(pref_name, option["default"].to_bool()),
					boost::bind(set_ptr, pref_name, _1),
					*main_grid);

				bind_status_label(toggle_box, "value", *main_grid);

				break;
			}

			case ADVANCED_PREF_TYPE::SLIDER: {
				tslider* setter_widget = new tslider;
				setter_widget->set_definition("minimal");
				setter_widget->set_id("setter");
				// Maximum must be set first or this will assert
				setter_widget->set_maximum_value(option["max"].to_int());
				setter_widget->set_minimum_value(option["min"].to_int());
				setter_widget->set_step_size(
					option["step"].empty() ? 1 : option["step"].to_int());

				delete details_grid->swap_child("setter", setter_widget, true);

				// Needed to disambiguate overloaded function
				typedef void (*setter) (const std::string &, int);
				setter set_ptr = &preferences::set;

				setup_single_slider("setter",
					lexical_cast_default<int>(get(pref_name), option["default"].to_int()),
					boost::bind(set_ptr, pref_name, _1),
					*details_grid);

				bind_status_label(*setter_widget, "value", *main_grid);

				break;
			}

			case ADVANCED_PREF_TYPE::COMBO: {
				combo_data combo_options;

				BOOST_FOREACH(const config& choice, option.child_range("option"))
				{
					combo_options.first.push_back(choice["name"]);
					combo_options.second.push_back(choice["id"]);
				}

				const unsigned selected = std::find(
					combo_options.second.begin(), combo_options.second.end(), 
					get(pref_name, option["default"].str())) - combo_options.second.begin();

				tcombobox* setter_widget = new tcombobox;
				setter_widget->set_definition("default");
				setter_widget->set_id("setter");

				delete details_grid->swap_child("setter", setter_widget, true);

				// Needed to disambiguate overloaded function
				typedef void (*setter) (const std::string &, const std::string &);
				setter set_ptr = &preferences::set;

				setup_combobox("setter",
					combo_options, selected,
					boost::bind(set_ptr, pref_name, _1),
					*details_grid);

				bind_status_label(*setter_widget, "value", *main_grid);

				break;
			}

			case ADVANCED_PREF_TYPE::SPECIAL: {
				//main_grid->remove_child("setter");

				timage* value_widget = new timage;
				value_widget->set_definition("default");
				value_widget->set_label("icons/arrows/arrows_blank_right_25.png~CROP(3,3,18,18)");

				delete main_grid->swap_child("value", value_widget, true);

				break;
			}
		}
	}

#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(advanced, boost::bind(
		&tpreferences::on_advanced_prefs_list_select, 
		this, 
		boost::ref(advanced),
		boost::ref(window)));
#else
	advanced.set_callback_value_change(make_dialog_callback(
		boost::bind(
		&tpreferences::on_advanced_prefs_list_select, 
		this, 
		boost::ref(advanced),
		boost::ref(window))));
#endif

	advanced.select_row(0);
}

void tpreferences::on_advanced_prefs_list_select(tlistbox& list, twindow& window)
{
	const int selected_row = list.get_selected_row();

	const ADVANCED_PREF_TYPE& selected_type = ADVANCED_PREF_TYPE::string_to_enum(
		adv_preferences_cfg_[selected_row]["type"].str());

	const std::string& selected_field = adv_preferences_cfg_[selected_row]["field"].str();

	if(selected_type == ADVANCED_PREF_TYPE::SPECIAL) {
		if (selected_field == "advanced_graphic_options") {
			gui2::tadvanced_graphics_options::display(window.video());
		}

		if (selected_field == "orb_color") {
			// TODO
		}

		// Add more options here as needed
	}

	if(selected_type != ADVANCED_PREF_TYPE::TOGGLE && selected_type != ADVANCED_PREF_TYPE::SPECIAL) {
		find_widget<twidget>(get_advanced_row_grid(list, selected_row), "prefs_setter_grid", false)
			.set_visible(tcontrol::tvisible::visible);
	}

	if(last_selected_item_ != selected_row) {
		find_widget<twidget>(get_advanced_row_grid(list, last_selected_item_), "prefs_setter_grid", false)
			.set_visible(tcontrol::tvisible::invisible);

		last_selected_item_ = selected_row;
	}
}

void tpreferences::add_pager_row(tlistbox& selector, const std::string& icon, const std::string& label)
{
	std::map<std::string, string_map> data;
	data["icon"]["label"] = "icons/icon-" + icon;
	data["label"]["label"] = label;
	selector.add_row(data);
}

void tpreferences::add_tab(tlistbox& tab_bar, const std::string& label)
{
	std::map<std::string, string_map> data;
	data["tab_label"]["label"] = label;
	tab_bar.add_row(data);
}

void tpreferences::initialize_tabs(twindow& window)
{
	/** MULTIPLAYER TABS **/
	tlistbox& tabs_multiplayer = find_widget<tlistbox>(&window, "mp_tab", false);
	add_tab(tabs_multiplayer, _("Prefs tab^General"));
	add_tab(tabs_multiplayer, _("Prefs tab^Friends"));
	//add_tab(tabs_multiplayer, _("Prefs tab^Alerts"));

	tabs_multiplayer.set_callback_value_change(make_dialog_callback(
		boost::bind(&tpreferences::on_tab_select, this, _1, "mp_tab")));
}

void tpreferences::pre_show(CVideo& /*video*/, twindow& window)
{
	tlistbox& selector = find_widget<tlistbox>(&window, "selector", false);
	tstacked_widget& pager = find_widget<tstacked_widget>(&window, "pager", false);

#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(selector, boost::bind(
			&tpreferences::on_page_select,
			this,
			boost::ref(window)));
#else
	selector.set_callback_value_change(dialog_callback
			<tpreferences, &tpreferences::on_page_select>);
#endif
	window.keyboard_capture(&selector);

	add_pager_row(selector, "general.png", _("Prefs section^General"));
	add_pager_row(selector, "display.png", _("Prefs section^Display"));
	add_pager_row(selector, "music.png",  _("Prefs section^Sound"));
	add_pager_row(selector, "multiplayer.png", _("Prefs section^Multiplayer"));
	add_pager_row(selector, "advanced.png", _("Prefs section^Advanced"));

	// Initializes tabs for the various pages. This should be done before
	// setting up the member callbacks.
	initialize_tabs(window);

	// Initializes initial values and sets up callbacks. This needs to be
	// done before selecting the initial page, otherwise widgets from other 
	// pages cannot be found afterwards.
	initialize_members(window);

	assert(selector.get_item_count() == pager.get_layer_count());

	// Selects initial tab for each page and the initially displayed main
	// page. This should be done last after all intilization
	set_visible_page(window, 0, "mp_tab_pager");

	selector.select_row(0);
	pager.select_layer(0);
}

void tpreferences::set_visible_page(twindow& window, unsigned int page, const std::string& pager_id)
{
	find_widget<tstacked_widget>(&window, pager_id, false).select_layer(page);
}

void tpreferences::single_toggle_callback(const ttoggle_button& widget,
		boost::function<void(bool)> setter)
{
	setter(widget.get_value_bool());
}

void tpreferences::toggle_slider_pair_callback(const ttoggle_button& toggle_widget,
		tslider& slider_widget, boost::function<void(bool)> setter)
{
	const bool ison = toggle_widget.get_value_bool();
	setter(ison);

	slider_widget.set_active(ison);
}

void tpreferences::single_slider_callback(const tslider& widget,
		boost::function<void(int)> setter)
{
	setter(widget.get_value());
}

void tpreferences::simple_combobox_callback(const tcombobox& widget,
		boost::function<void(std::string)> setter, std::vector<std::string>& vec)
{
	const unsigned index = widget.get_value();
	setter(vec[index]);
}

template <typename T>
void tpreferences::status_label_callback(T& parent_widget,
		tcontrol& label_widget)
{
	label_widget.set_label(disambiguate_widget_value(parent_widget));
}

// Special fullsceen callback
void tpreferences::fullscreen_toggle_callback(twindow& window)
{
	const bool ison =
		find_widget<ttoggle_button>(&window, "fullscreen", false).get_value_bool();
	window.video().set_fullscreen(ison);

	tcombobox& res_list = find_widget<tcombobox>(&window, "resolution_set", false);
	set_resolution_list(res_list, window.video());
	res_list.set_active(!ison);
}

void tpreferences::handle_res_select(twindow& window)
{
	tcombobox& res_list = find_widget<tcombobox>(&window, "resolution_set", false);
	const int choice = res_list.get_value();

	if (resolutions_[static_cast<size_t>(choice)] == window.video().current_resolution()) {
		return;
	}

	window.video().set_resolution(resolutions_[static_cast<size_t>(choice)]);
	set_resolution_list(res_list, window.video());
}

// Special Accelerated Speed slider callback
void tpreferences::accl_speed_slider_callback(tslider& slider)
{
	const int index = slider.get_value();
	set_turbo_speed(lexical_cast<double>(accl_speeds_[index - 1]));
}

// Special Max Autosaves slider callback
void tpreferences::max_autosaves_slider_callback(tslider& slider, tcontrol& status_label)
{
	status_label.set_label(get_max_autosaves_status_label(slider));
}

void tpreferences::toggle_radio_callback(
		const std::vector<std::pair<ttoggle_button*, int> >& vec,
		int& value,
		ttoggle_button* active)
{
	FOREACH(const AUTO & e, vec)
	{
		ttoggle_button* const b = e.first;
		if(b == NULL) {
			continue;
		} else if(b == active && !b->get_value()) {
			b->set_value(true);
		} else if(b == active) {
			value = e.second;
			_set_lobby_joins(value);
		} else if(b != active && b->get_value()) {
			b->set_value(false);
		}
	}
}

void tpreferences::on_page_select(twindow& window)
{
	const int selected_row =
		std::max(0, find_widget<tlistbox>(&window, "selector", false).get_selected_row());
	set_visible_page(window, static_cast<unsigned int>(selected_row), "pager");
}

void tpreferences::on_tab_select(twindow& window, const std::string& widget_id)
{
	const int selected_row =
		std::max(0, find_widget<tlistbox>(&window, widget_id, false).get_selected_row());
	set_visible_page(window, static_cast<unsigned int>(selected_row), (widget_id + "_pager"));
}

} // end namespace gui2
