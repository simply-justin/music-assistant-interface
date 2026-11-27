// Sheets: speaker picker, volume, track options (with "Play from"), add to playlist, new playlist,
// and the generic guarded confirm.
#include <algorithm>

#include "../shell.hpp"
#include "../text.hpp"

namespace ui {

namespace {

lv_obj_t* sheet_header(lv_obj_t* sheet, const std::string& title, const std::string& sub = {}, kit::Action on_back = {}) {
    lv_obj_t* h = kit::row(sheet, 16);
    lv_obj_set_width(h, lv_pct(100));
    if (on_back) kit::icon_button(h, icon::chevron_left, std::move(on_back), 56, 28, true);
    lv_obj_t* col = kit::column(h, 2);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);
    kit::text(col, title, theme::h2(), theme::text());
    if (!sub.empty()) kit::text(col, sub, theme::meta(), theme::text2());
    kit::icon_button(h, icon::close, [] { kit::close_sheets(); }, 56, 28, true);
    return h;
}

lv_obj_t* menu_row(lv_obj_t* parent, const char* glyph, const std::string& label, kit::Action action) {
    lv_obj_t* r = kit::row(parent, 16);
    lv_obj_set_size(r, lv_pct(100), 64);
    lv_obj_set_style_pad_hor(r, 16, 0);
    lv_obj_set_style_radius(r, theme::r_control(), 0);
    lv_obj_set_style_bg_color(r, theme::elevated(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    kit::icon(r, glyph, 28, theme::text2());
    kit::text(r, label, theme::body(), theme::text());
    kit::on_click(r, std::move(action));
    return r;
}

// `name` empty: the top row (its name is the sheet title). `whole_group`: group_volume for a group.
void volume_row(lv_obj_t* parent, const std::string& name, const std::string& player_id, int level, bool whole_group) {
    lv_obj_t* r = kit::row(parent, 16);
    lv_obj_set_size(r, lv_pct(100), 64);
    if (!name.empty()) lv_obj_set_width(kit::text(r, name, theme::body(), theme::text()), 180);
    auto step = [player_id, whole_group](int delta, lv_obj_t* slider) {
        int v = std::clamp(static_cast<int>(lv_slider_get_value(slider)) + delta, 0, 100);
        lv_slider_set_value(slider, v, LV_ANIM_OFF);
        send(app::cmd::SetVolume{player_id, v, whole_group});
    };
    lv_obj_t* minus = kit::icon_button(r, icon::minus, {}, 56, 28, true);
    lv_obj_t* s = kit::slider(r, level, [player_id, whole_group](int v) { send(app::cmd::SetVolume{player_id, v, whole_group}); });
    lv_obj_set_flex_grow(s, 1);
    lv_obj_t* plus = kit::icon_button(r, icon::plus, {}, 56, 28, true);
    kit::on_click(minus, [step, s] { step(-5, s); });
    kit::on_click(plus, [step, s] { step(5, s); });
    lv_obj_t* v = kit::text(r, std::to_string(level), name.empty() ? theme::h1() : theme::h2(), theme::text());
    lv_obj_set_width(v, 72);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_add_event_cb(s, [](lv_event_t* e) {
        auto* label = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
        lv_label_set_text_fmt(label, "%d", static_cast<int>(lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e)))));
    }, LV_EVENT_VALUE_CHANGED, v);
}

}  // namespace

namespace {
const core::PlayerState* player_by_id(const std::string& id) {
    if (!state().players) return nullptr;
    for (const auto& p : state().players->players)
        if (p.player_id == id) return &p;
    return nullptr;
}

std::string names(const std::vector<std::string>& ids, const std::string& skip) {
    std::string out;
    for (const auto& id : ids) {
        if (id == skip) continue;
        const core::PlayerState* p = player_by_id(id);
        out += (out.empty() ? "" : ", ") + (p ? p->name : id);
    }
    return out;
}

std::string status(const core::PlayerState& p) {
    if (!p.available) return tr(i18n::Str::spk_offline);
    if (!p.synced_to.empty()) {
        const core::PlayerState* l = player_by_id(p.synced_to);
        return i18n::tr(i18n::Str::spk_joined, l ? l->name : p.synced_to);
    }
    if (p.group_members.size() > 1) return i18n::tr(i18n::Str::spk_group_with, names(p.group_members, p.player_id));
    if (p.state == core::PlaybackState::Playing)
        return p.media_title.empty() ? tr(i18n::Str::side_playing) : i18n::tr(i18n::Str::spk_playing, p.media_title);
    if (p.state == core::PlaybackState::Paused) return tr(i18n::Str::home_paused);
    return tr(i18n::Str::side_idle);
}
}  // namespace

// One speaker: icon tile, name, status; the selected one is a filled block with a check.
lv_obj_t* speaker_row(lv_obj_t* parent, const core::PlayerState& p, bool selected) {
    lv_obj_t* r = kit::row(parent, 16);
    lv_obj_set_size(r, lv_pct(100), 72);
    lv_obj_set_style_pad_hor(r, 12, 0);
    lv_obj_set_style_radius(r, theme::r_card(), 0);
    lv_obj_set_style_bg_color(r, theme::elevated(), 0);
    lv_obj_set_style_bg_opa(r, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(r, theme::elevated(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_t* tile = kit::row(r, 0);
    lv_obj_set_size(tile, 48, 48);
    lv_obj_set_style_radius(tile, theme::r_control(), 0);
    lv_obj_set_style_bg_color(tile, selected ? theme::accent() : theme::surface(), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    kit::icon(tile, icon::speaker, 26, selected ? theme::on_accent() : p.available ? theme::text2() : theme::text3());
    lv_obj_t* col = kit::column(r, 4);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);
    kit::text(col, p.name, theme::body(), p.available ? theme::text() : theme::text3());
    lv_obj_t* st = kit::row(col, 6);
    lv_obj_set_width(st, lv_pct(100));
    if (p.state == core::PlaybackState::Playing && p.available) kit::equalizer(st, theme::accent(), true);
    lv_obj_t* sl = kit::text(st, status(p), theme::label(), theme::text2());
    lv_obj_set_flex_grow(sl, 1);
    lv_obj_set_width(sl, 1);
    if (selected) kit::icon(r, icon::check, 26, theme::accent());
    std::string id = p.player_id;
    kit::on_click(r, [id] { send(app::cmd::SelectPlayer{id}); });
    return r;
}

void open_speakers() {
    kit::close_sheets();
    lv_obj_t* s = kit::sheet(640);
    sheet_header(s, tr(i18n::Str::speakers_title));
    const auto* pv = state().players.get();
    if (!pv) return;
    lv_obj_t* list = kit::column(s, 4);
    lv_obj_set_width(list, lv_pct(100));
    for (const auto& p : pv->players) {
        lv_obj_t* r = speaker_row(list, p, p.player_id == pv->selected_id);
        kit::on_click(r, [] { kit::close_sheets(); });  // picking a speaker also closes the sheet
    }
    // Grouping belongs to the chosen speaker: one quiet row under the list, only when MA allows it.
    const core::PlayerState* sel = state().selected();
    if (sel && sel->available && sel->synced_to.empty() && !sel->can_group_with.empty()) {
        lv_obj_t* div = lv_obj_create(s);
        lv_obj_remove_style_all(div);
        lv_obj_remove_flag(div, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(div, lv_pct(100), 1);
        lv_obj_set_style_bg_color(div, theme::hairline(), 0);
        lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
        std::string id = sel->player_id;
        bool grouped = sel->group_members.size() > 1;
        lv_obj_t* g = nav_row(s, icon::plus, tr(grouped ? i18n::Str::spk_edit_group : i18n::Str::spk_add_speakers),
                              i18n::tr(i18n::Str::spk_group_sub, sel->name), [id] { open_group(id); });
        lv_obj_set_style_pad_hor(g, 12, 0);
    }
}

void open_group(const std::string& leader_id) {
    kit::close_sheets();
    const core::PlayerState* leader = player_by_id(leader_id);
    if (!leader) return;
    lv_obj_t* s = kit::sheet(640);
    sheet_header(s, tr(i18n::Str::group_title), i18n::tr(i18n::Str::group_sub, leader->name));
    // Who is ticked: starts as the current group; edited in place (no rebuild under the finger).
    auto chosen = std::make_shared<std::vector<std::string>>();
    for (const auto& id : leader->group_members)
        if (id != leader_id) chosen->push_back(id);
    lv_obj_t* list = kit::column(s, 0);
    lv_obj_set_width(list, lv_pct(100));
    {
        lv_obj_t* r = kit::row(list, 14);
        lv_obj_set_size(r, lv_pct(100), 60);
        lv_obj_set_style_pad_hor(r, 12, 0);
        kit::icon(r, icon::check, 22, theme::text3());
        kit::icon(r, icon::speaker, 28, theme::text2());
        lv_obj_t* c = kit::column(r, 2);
        kit::text(c, leader->name, theme::meta(), theme::text());
        kit::text(c, tr(i18n::Str::group_leader), theme::label(), theme::text3());
    }
    for (const auto& id : leader->can_group_with) {
        const core::PlayerState* p = player_by_id(id);
        if (!p) continue;
        bool on = std::find(chosen->begin(), chosen->end(), id) != chosen->end();
        lv_obj_t* r = kit::row(list, 14);
        lv_obj_set_size(r, lv_pct(100), 60);
        lv_obj_set_style_pad_hor(r, 12, 0);
        lv_obj_set_style_radius(r, theme::r_control(), 0);
        lv_obj_set_style_bg_color(r, theme::elevated(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_t* box = lv_obj_create(r);
        lv_obj_remove_style_all(box);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(box, 22, 22);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_t* tick = kit::icon(box, icon::check, 22, theme::on_accent());
        lv_obj_center(tick);
        auto paint = [box, tick](bool v) {
            lv_obj_set_style_border_color(box, v ? theme::accent() : theme::text3(), 0);
            lv_obj_set_style_bg_color(box, theme::accent(), 0);
            lv_obj_set_style_bg_opa(box, v ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            if (v) lv_obj_remove_flag(tick, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(tick, LV_OBJ_FLAG_HIDDEN);
        };
        paint(on);
        kit::icon(r, icon::speaker, 28, theme::text2());
        kit::text(r, p->name, theme::meta(), theme::text());
        kit::on_click(r, [chosen, id, paint] {
            auto it = std::find(chosen->begin(), chosen->end(), id);
            bool now_on = it == chosen->end();
            if (now_on) chosen->push_back(id);
            else chosen->erase(it);
            paint(now_on);
        });
    }
    if (leader->can_group_with.empty()) kit::text(s, tr(i18n::Str::group_none), theme::meta(), theme::text2(), true);
    lv_obj_t* buttons = kit::row(s, 12);
    lv_obj_set_width(buttons, lv_pct(100));
    if (leader->group_members.size() > 1)
        kit::button(buttons, kit::Button::Ghost, nullptr, tr(i18n::Str::group_ungroup), [leader_id] {
            send(app::cmd::Ungroup{leader_id});
            kit::close_sheets();
        });
    kit::spacer(buttons);
    kit::button(buttons, kit::Button::Secondary, nullptr, tr(i18n::Str::edit_delete_keep), [] { kit::close_sheets(); });
    kit::button(buttons, kit::Button::Primary, nullptr, tr(i18n::Str::group_save), [leader_id, chosen] {
        send(app::cmd::SetGroup{leader_id, *chosen});
        kit::close_sheets();
    });
}

void open_volume() {
    kit::close_sheets();
    const core::PlayerState* p = state().selected();
    if (!p) return;
    lv_obj_t* s = kit::sheet(760);
    sheet_header(s, p->name);
    bool group = p->group_members.size() > 1;  // MA may list a lone speaker as its own one-member group
    volume_row(s, "", p->player_id, group ? p->group_volume : p->volume_level, group);
    if (group && state().players) {
        kit::overline(s, tr(i18n::Str::volume_each_speaker));
        for (const auto& member_id : p->group_members)
            for (const auto& m : state().players->players)
                if (m.player_id == member_id) volume_row(s, m.name, m.player_id, m.volume_level, false);
    }
    kit::text(s, tr(group ? i18n::Str::volume_hint : i18n::Str::volume_hint_single), theme::meta(), theme::text3());
    // Closes itself once nobody has touched the screen for 4 s (a slider being dragged counts).
    lv_timer_t* t = lv_timer_create([](lv_timer_t* timer) {
        if (lv_display_get_inactive_time(nullptr) < 4000) return;
        lv_timer_pause(timer);  // the sheet's DELETE handler below deletes it
        lv_obj_delete_async(static_cast<lv_obj_t*>(lv_timer_get_user_data(timer)));
    }, 250, lv_obj_get_parent(s));
    lv_obj_add_event_cb(lv_obj_get_parent(s), [](lv_event_t* e) {
        if (auto* timer = static_cast<lv_timer_t*>(lv_event_get_user_data(e))) lv_timer_delete(timer);
    }, LV_EVENT_DELETE, t);
}

void open_track_menu(const core::MediaItem& item) {
    kit::close_sheets();
    lv_obj_t* s = kit::sheet(860);
    sheet_header(s, item.name, kit::subtitle(item) + (item.duration_s ? " · " + duration(item.duration_s) : ""));
    lv_obj_t* cols = kit::row(s, 28);
    lv_obj_set_width(cols, lv_pct(100));
    lv_obj_set_flex_align(cols, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* actions = kit::column(cols, 2);
    lv_obj_set_flex_grow(actions, 1);
    auto it = item;
    menu_row(actions, icon::play, tr(i18n::Str::menu_play_now), [it] { play(it); kit::close_sheets(); });
    if (item.type != core::MediaType::Radio) {
        menu_row(actions, icon::play_next, tr(i18n::Str::menu_play_next), [it] {
            send(app::cmd::AddToQueue{it, true});
            kit::close_sheets();
        });
        menu_row(actions, icon::add_to_queue, tr(i18n::Str::menu_add_to_queue), [it] {
            send(app::cmd::AddToQueue{it});
            kit::close_sheets();
        });
    }
    if (item.type == core::MediaType::Track)
        menu_row(actions, icon::playlist_add, tr(i18n::Str::menu_add_to_playlist), [it] { open_add_to_playlist(it); });
    if (item.type == core::MediaType::Track)
        menu_row(actions, icon::radio, tr(i18n::Str::menu_start_radio), [it] {
            play(it, std::nullopt, true);
            kit::close_sheets();
        });
    bool fav = state().favourite(it);
    menu_row(actions, fav ? icon::heart_filled : icon::heart,
             tr(fav ? i18n::Str::menu_remove_favourite : i18n::Str::menu_add_favourite), [it, fav] {
                 kit::close_sheets();
                 set_favourite(it, !fav);
             });
    if (item.album) {
        core::MediaItem album;
        album.type = core::MediaType::Album;
        album.item_id = item.album->item_id;
        album.provider = item.album->provider;
        album.name = item.album->name;
        album.uri = item.album->uri;
        menu_row(actions, icon::album, tr(i18n::Str::menu_go_album), [album] { open_detail(album); });
    }
    if (!item.artists.empty()) {
        core::MediaItem artist;
        artist.type = core::MediaType::Artist;
        artist.item_id = item.artists[0].item_id;
        artist.provider = item.artists[0].provider;
        artist.name = item.artists[0].name;
        artist.uri = item.artists[0].uri;
        menu_row(actions, icon::artist, tr(i18n::Str::menu_go_artist), [artist] { open_detail(artist); });
    }

    // "Play from": the same recording on other sources (duplicates merged by core).
    if (!item.alternatives.empty()) {
        lv_obj_t* div = lv_obj_create(cols);
        lv_obj_remove_style_all(div);
        lv_obj_set_size(div, 1, 300);
        lv_obj_set_style_bg_color(div, theme::elevated(), 0);
        lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
        lv_obj_t* from = kit::column(cols, 10);
        lv_obj_set_width(from, 340);
        kit::overline(from, tr(i18n::Str::menu_play_from));
        std::vector<core::MediaItem> sources{item};
        sources.insert(sources.end(), item.alternatives.begin(), item.alternatives.end());
        for (const auto& src : sources) {
            lv_obj_t* r = kit::row(from, 14);
            lv_obj_set_size(r, lv_pct(100), 76);
            lv_obj_set_style_pad_hor(r, 16, 0);
            lv_obj_set_style_radius(r, theme::r_control(), 0);
            lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(r, &src == &sources[0] ? theme::elevated() : theme::surface(), 0);
            lv_obj_set_style_bg_color(r, lv_color_mix(lv_color_white(), theme::elevated(), 31), LV_STATE_PRESSED);
            kit::badge(r, src.badge_provider());
            lv_obj_t* c = kit::column(r, 2);
            lv_obj_set_flex_grow(c, 1);
            lv_obj_set_width(c, 1);
            kit::text(c, src.in_library() ? tr(i18n::Str::menu_your_library) : kit::provider_name(src.badge_provider()),
                      theme::body(), theme::text());
            kit::text(c, src.duration_s ? duration(src.duration_s) : "", theme::meta(), theme::text2());
            auto pick = src;
            pick.alternatives.clear();
            kit::on_click(r, [pick] { play(pick); kit::close_sheets(); });
        }
    }
}

// Long-press on a Home favourite: what it is and what you can do with it.
void open_favourite_preview(const core::MediaItem& item) {
    kit::close_sheets();
    lv_obj_t* s = kit::sheet(760);
    lv_obj_t* head = kit::row(s, 24);
    lv_obj_set_width(head, lv_pct(100));
    kit::art(head, item, 120, theme::r_art(), theme::surface());
    lv_obj_t* col = kit::column(head, 6);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_width(col, 1);
    kit::text(col, item.name, theme::h2(), theme::text());
    lv_obj_t* sub = kit::row(col, 8);
    kit::text(sub, kit::subtitle(item), theme::body(), theme::text2());
    kit::badge(sub, item.badge_provider());
    kit::icon_button(head, icon::close, [] { kit::close_sheets(); }, 56, 28, true);
    auto it = item;
    lv_obj_t* main = kit::row(s, 12);
    kit::button(main, kit::Button::Primary, icon::play, tr(i18n::Str::detail_play), [it] { play(it); kit::close_sheets(); });
    if (item.type != core::MediaType::Radio && item.type != core::MediaType::Track)
        kit::button(main, kit::Button::Secondary, icon::shuffle, tr(i18n::Str::queue_shuffle), [it] {
            send(app::cmd::SetShuffle{true});
            play(it);
            kit::close_sheets();
        });
    lv_obj_t* actions = kit::column(s, 2);
    lv_obj_set_width(actions, lv_pct(100));
    if (item.type != core::MediaType::Radio) {
        menu_row(actions, icon::play_next, tr(i18n::Str::menu_play_next), [it] { send(app::cmd::AddToQueue{it, true}); kit::close_sheets(); });
        menu_row(actions, icon::add_to_queue, tr(i18n::Str::menu_add_to_queue), [it] { send(app::cmd::AddToQueue{it}); kit::close_sheets(); });
    }
    if (item.type == core::MediaType::Album || item.type == core::MediaType::Playlist || item.type == core::MediaType::Artist)
        menu_row(actions, icon::chevron_right, tr(i18n::Str::preview_open), [it] { open_detail(it); });
    menu_row(actions, icon::heart_filled, tr(i18n::Str::menu_remove_favourite), [it] {
        kit::close_sheets();
        set_favourite(it, false);
    });
}

void open_add_to_playlist(const core::MediaItem& item) {
    kit::close_sheets();
    send(app::cmd::LoadPlaylists{});
    lv_obj_t* s = kit::sheet(760);
    auto it = item;
    sheet_header(s, tr(i18n::Str::menu_add_to_playlist), item.name, [it] { open_track_menu(it); });
    lv_obj_t* add = menu_row(s, icon::plus, tr(i18n::Str::playlist_new), [it] { open_new_playlist(it); });
    lv_obj_set_style_text_color(lv_obj_get_child(add, 0), theme::accent(), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(add, 1), theme::accent(), 0);
    lv_obj_t* list = kit::scroll_column(s, 2);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_style_max_height(list, 420, 0);
    if (state().playlists) {
        for (const auto& pl : state().playlists->playlists) {
            if (!pl.editable) continue;  // only playlists Music Assistant can change
            auto playlist = pl;
            kit::RowOptions o;
            o.show_duration = false;
            o.on_tap = [playlist, it] {
                send(app::cmd::AddToPlaylist{playlist, it});
                kit::close_sheets();
            };
            kit::media_row(list, pl, o);
        }
    }
    kit::text(s, tr(i18n::Str::playlist_only_editable), theme::meta(), theme::text3());
}

void open_new_playlist(std::optional<core::MediaItem> first_item) {
    kit::close_sheets();
    lv_obj_t* s = kit::sheet(760);
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 40);
    sheet_header(s, tr(i18n::Str::playlist_new));
    lv_obj_t* ta = lv_textarea_create(s);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_size(ta, lv_pct(100), 64);
    lv_obj_set_style_radius(ta, theme::r_control(), 0);
    lv_obj_set_style_bg_color(ta, theme::elevated(), 0);
    lv_obj_set_style_border_color(ta, theme::accent(), 0);
    lv_obj_set_style_border_width(ta, 2, 0);
    lv_obj_set_style_text_font(ta, theme::h2(), 0);
    lv_obj_set_style_text_color(ta, theme::text(), 0);
    lv_obj_set_style_pad_hor(ta, 20, 0);
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    lv_obj_t* buttons = kit::row(s, 16);
    lv_obj_set_width(buttons, lv_pct(100));
    lv_obj_t* cancel = kit::button(buttons, kit::Button::Secondary, nullptr, tr(i18n::Str::edit_delete_keep), [] { kit::close_sheets(); });
    lv_obj_set_flex_grow(cancel, 1);
    std::string label = first_item ? i18n::tr(i18n::Str::playlist_create_add, first_item->name) : tr(i18n::Str::playlist_create);
    lv_obj_t* go = kit::button(buttons, kit::Button::Primary, nullptr, label, [ta, first_item] {
        std::string name = lv_textarea_get_text(ta);
        if (name.empty()) return;
        send(app::cmd::CreatePlaylist{name, first_item});
        kit::close_sheets();
    });
    lv_obj_set_flex_grow(go, 1);

    // Full-width keyboard along the bottom of the screen (same layer as the sheet).
    lv_obj_t* kb = kit::keyboard(lv_obj_get_parent(s), ta, tr(i18n::Str::key_done),
                                 tr(i18n::Str::key_space), {});
    lv_obj_add_flag(kb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void confirm(const std::string& title, const std::string& body, const std::string& keep, const std::string& go,
             kit::Action on_go) {
    kit::close_sheets();
    lv_obj_t* s = kit::sheet(600);
    lv_obj_set_flex_align(s, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s, 40, 0);
    kit::icon(s, icon::trash, 32, theme::danger_text());
    lv_obj_t* t = kit::text(s, title, theme::h1(), theme::text(), true);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t* b = kit::text(s, body, theme::lead(), theme::text2(), true);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t* buttons = kit::row(s, 16);
    lv_obj_set_width(buttons, lv_pct(100));
    lv_obj_t* k = kit::button(buttons, kit::Button::Secondary, nullptr, keep, [] { kit::close_sheets(); });
    lv_obj_set_flex_grow(k, 1);
    lv_obj_set_height(k, 64);
    lv_obj_t* g = kit::button(buttons, kit::Button::Danger, nullptr, go, [on_go] {
        kit::close_sheets();
        on_go();
    });
    lv_obj_set_flex_grow(g, 1);
}

}  // namespace ui
