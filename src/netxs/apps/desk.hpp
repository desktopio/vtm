// Copyright (c) NetXS Group.
// Licensed under the MIT license.

#pragma once

// desk: Taskbar.
namespace netxs::app::desk
{
    static constexpr auto id = "desk";
    static constexpr auto desc = "Taskbar menu";

    struct spec
    {
        text   menuid{};
        text    alias{};
        bool   hidden{};
        text    label{};
        text    notes{};
        text    title{};
        text   footer{};
        rgba      bgc{};
        rgba      fgc{};
        twod  winsize{};
        twod  wincoor{};
        bool slimmenu{};
        bool splitter{};
        text   hotkey{};
        text      cwd{};
        text     type{};
        text    param{};
        text    patch{};
    };

    using menu = std::unordered_map<text, spec>;
    using usrs = std::list<sptr<base>>;
    using apps = generics::imap<text, std::pair<bool, usrs>>;

    struct events
    {
        EVENTPACK( events, ui::e2::extra::slot2 )
        {
            EVENT_XS( usrs, sptr<desk::usrs> ), // list of connected users.
            EVENT_XS( apps, sptr<desk::apps> ), // list of running apps.
            EVENT_XS( menu, sptr<desk::menu> ), // list of registered apps.
            GROUP_XS( ui  , text             ),

            SUBSET_XS( ui )
            {
                EVENT_XS( selected, text        ),
                GROUP_XS( focus   , input::hids ),

                SUBSET_XS( focus )
                {
                    EVENT_XS( set, input::hids ), // Request focus.
                    EVENT_XS( off, input::hids ), // Request unfocus.
                };
            };
        };
    };

    namespace
    {
        auto app_template = [](auto& data_src, auto const& utf8)
        {
            auto& tall = skin::globals().menuwide;
            auto danger_color    = skin::globals().danger;
            auto highlight_color = skin::globals().highlight;
            auto c4 = cell{}.bgc(highlight_color.bgc());
            auto x4 = cell{ c4 }.bga(0x00);
            auto c5 = danger_color;
            auto x5 = cell{ c5 }.alpha(0x00);
            auto fastfader = skin::globals().fader_fast;
            auto fader = skin::globals().fader_time;
            auto item_area = ui::pads::ctor(dent{ 1,0,tall,0 }, dent{ 0,0,0,tall })
                ->plugin<pro::fader>(x4, c4, fastfader)
                ->plugin<pro::notes>(" Application window:              \n"
                                     "   Left click to go to the window \n"
                                     "   Right click to pull the window ")
                ->invoke([&](auto& boss)
                {
                    auto data_src_shadow = ptr::shadow(data_src);
                    auto boss_ptr_shadow = ptr::shadow(boss.This());
                    boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear, -, (data_src_shadow))
                    {
                        if (auto data_src = data_src_shadow.lock())
                        {
                            auto& window = *data_src;
                            auto  center = window.base::area().center();
                            window.SIGNAL(tier::request, e2::form::prop::zorder, zpos_state, ());
                            gear.owner.SIGNAL(tier::request, e2::form::prop::viewport, viewport, ());

                            if (viewport.hittest(center)        // Minimize if visible,
                             && zpos_state != zpos::hidden      // not minimized,
                             && pro::focus::test(window, gear)) // and focused.
                            {
                                if (gear.meta(hids::anyCtrl)) pro::focus::off(data_src, gear.id); // Remove focus if Ctrl pressed.
                                else                          window.SIGNAL(tier::release, e2::form::layout::minimize, gear);
                            }
                            else
                            {
                                window.RISEUP(tier::preview, e2::form::layout::expose, area, ());
                                gear.owner.SIGNAL(tier::release, e2::form::layout::jumpto, window);
                                if (zpos_state == zpos::hidden) // Restore if hidden.
                                {
                                    window.SIGNAL(tier::release, e2::form::layout::minimize, gear);
                                }
                                else pro::focus::set(data_src, gear.id, gear.meta(hids::anyCtrl) ? pro::focus::solo::off
                                                                                                 : pro::focus::solo::on, pro::focus::flip::off);
                            }
                            gear.dismiss();
                        }
                    };
                    boss.LISTEN(tier::release, hids::events::mouse::button::click::right, gear, -, (data_src_shadow))
                    {
                        if (auto data_src = data_src_shadow.lock())
                        {
                            auto& window = *data_src;
                            window.SIGNAL(tier::request, e2::form::prop::zorder, zpos_state, ());
                            window.RISEUP(tier::preview, e2::form::layout::expose, area, ());
                            boss.SIGNAL(tier::anycast, e2::form::prop::viewport, viewport, ());
                            window.SIGNAL(tier::preview, e2::form::layout::appear, center, (gear.area().coor + viewport.center())); // Pull window.
                            if (zpos_state == zpos::hidden) // Restore if minimized.
                            {
                                window.SIGNAL(tier::release, e2::form::layout::minimize, gear);
                            }
                            else pro::focus::set(data_src, gear.id, pro::focus::solo::on, pro::focus::flip::off);
                            gear.dismiss();
                        }
                    };
                    boss.LISTEN(tier::release, e2::form::state::mouse, hits, -, (data_src_shadow))
                    {
                        if (auto data_src = data_src_shadow.lock())
                        {
                            data_src->SIGNAL(tier::release, e2::form::state::highlight, !!hits);
                        }
                    };
                });
            auto label_area = item_area->attach(ui::fork::ctor());
            auto mark_app = label_area->attach(slot::_1, ui::fork::ctor());
            auto mark = mark_app->attach(slot::_1, ui::pads::ctor(dent{ 2,1,0,0 }, dent{ 0,0,0,0 }))
                ->attach(ui::item::ctor(ansi::fgx(0xFF00ff00).add("‣"), faux));
            auto app_label = mark_app->attach(slot::_2, ui::item::ctor(ansi::fgc(whitelt).add(utf8).mgl(0).wrp(wrap::off).jet(bias::left), true, true));
            auto app_close_area = label_area->attach(slot::_2, ui::pads::ctor(dent{ 0,0,0,0 }, dent{ 0,0,tall,tall }))
                ->template plugin<pro::fader>(x5, c5, fader)
                ->template plugin<pro::notes>(" Close application window ")
                ->invoke([&](auto& boss)
                {
                    auto data_src_shadow = ptr::shadow(data_src);
                    boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear, -, (data_src_shadow))
                    {
                        if (auto data_src = data_src_shadow.lock())
                        {
                            data_src->SIGNAL(tier::anycast, e2::form::proceed::quit::one, faux); // Show closing process.
                            gear.dismiss();
                        }
                    };
                });
            auto app_close = app_close_area->attach(ui::item::ctor("  ×  ", faux));
            return item_area;
        };
        auto apps_template = [](auto& data_src, auto& apps_map_ptr)
        {
            auto& tall = skin::globals().menuwide;
            auto highlight_color = skin::globals().highlight;
            auto inactive_color  = skin::globals().inactive;
            auto selected_color  = skin::globals().selected;
            auto action_color    = skin::globals().action;
            auto danger_color    = skin::globals().danger;
            auto c3 = highlight_color;
            auto c9 = selected_color;
            auto x3 = cell{ c3 }.alpha(0x00);
            auto cA = inactive_color;
            auto c6 = action_color;
            auto x6 = cell{ c6 }.alpha(0x00);
            auto c1 = danger_color;
            auto x1 = cell{ c1 }.bga(0x00);

            auto apps = ui::list::ctor()
                ->invoke([&](auto& boss)
                {
                    boss.LISTEN(tier::release, e2::form::upon::vtree::attached, parent)
                    {
                        boss.RISEUP(tier::request, e2::data::changed, current_default, ());
                        boss.SIGNAL(tier::anycast, events::ui::selected, current_default);
                    };
                });

            auto def_note = text{" Application:                                   \n"
                                 "   Left click to start the application instance \n"
                                 "   Right click to set as default                "};
            data_src->RISEUP(tier::request, desk::events::menu, conf_list_ptr, ());
            if (!conf_list_ptr || !apps_map_ptr) return apps;
            auto& conf_list = *conf_list_ptr;
            auto& apps_map = *apps_map_ptr;
            //todo optimize: use a branch_template for sublist instead of recreating whole app list
            for (auto const& [class_id, stat_inst_ptr_list] : apps_map)
            {
                auto& [state, inst_ptr_list] = stat_inst_ptr_list;
                auto inst_id = class_id;
                auto& conf = conf_list[class_id];
                auto& obj_desc = conf.label;
                auto& obj_note = conf.notes;
                if (conf.splitter)
                {
                    auto item_area = apps->attach(ui::pads::ctor(dent{ 0,0,0,tall }, dent{ 0,0,tall,0 }))
                        ->attach(ui::item::ctor(obj_desc, true, faux, true))
                        ->colors(cA.fgc(), cA.bgc())
                        ->template plugin<pro::notes>(obj_note);
                    continue;
                }
                auto item_area = apps->attach(ui::pads::ctor(dent{ 0,0,0,tall }, dent{ 0,0,tall,0 }));
                if (!state) item_area->depend_on_collection(inst_ptr_list); // Remove not pinned apps, like Info/About.
                auto block = item_area->attach(ui::fork::ctor(axis::Y));
                auto head_pads = block->attach(slot::_1, ui::fork::ctor(axis::X))
                    ->template plugin<pro::fader>(x3, c3, skin::globals().fader_fast)
                    ->template plugin<pro::notes>(obj_note.empty() ? def_note : obj_note)
                    ->invoke([&](auto& boss)
                    {
                        //boss.mouse.take_all_events(faux);
                        //auto boss_shadow = ptr::shadow(boss.This());
                        //auto data_src_shadow = ptr::shadow(data_src);
                        boss.LISTEN(tier::release, hids::events::mouse::button::click::right, gear, -, (inst_id))
                        {
                            boss.SIGNAL(tier::anycast, events::ui::selected, inst_id);
                        };
                        boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear, -, (inst_id, group_focus = faux, offset = dot_00))
                        {
                            if (gear.meta(hids::anyCtrl | hids::anyAlt | hids::anyShift | hids::anyWin)) // Not supported with any modifier but Ctrl.
                            {
                                if (gear.meta(hids::anyCtrl)) // Toggle group focus.
                                {
                                    group_focus = !group_focus;
                                    if (group_focus) boss.SIGNAL(tier::release, events::ui::focus::set, gear);
                                    else             boss.SIGNAL(tier::release, events::ui::focus::off, gear);
                                }
                                gear.dismiss();
                                return;
                            }
                            boss.SIGNAL(tier::anycast, events::ui::selected, inst_id);
                            boss.SIGNAL(tier::anycast, e2::form::prop::viewport, viewport, ());
                            viewport.coor += gear.area().coor;
                            offset = (offset + dot_21 * 2) % (viewport.size * 7 / 32);
                            gear.slot.coor = viewport.coor + offset + viewport.size * 1 / 32;
                            gear.slot.size = viewport.size * 3 / 4;
                            gear.slot_forced = faux;
                            boss.RISEUP(tier::request, e2::form::proceed::createby, gear);
                            gear.dismiss();
                        };
                    });
                auto head_area = head_pads->attach(slot::_1, ui::pads::ctor(dent{ 0,0,0,0 }, dent{ 0,0,tall,tall }));
                auto count = inst_ptr_list.size();
                auto head_bttn = head_pads->attach(slot::_2, ui::pads::ctor(dent{ 0,0,0,0 }, dent{ 0,0,tall,tall }))
                    ->limits({ count ? -1 : 0,-1 }, { count ? -1 : 0,-1 });
                if (inst_ptr_list.size())
                {
                    auto bttn_rail = head_bttn->attach(ui::rail::ctor(axes::X_only, axes::all, axes::none))
                        ->limits({ 5,1 }, { 5,1 });
                    auto bttn_fork = bttn_rail->attach(ui::fork::ctor(axis::X));
                    auto fold_bttn = bttn_fork->attach(slot::_1, ui::item::ctor("  <  ", faux))
                        ->template plugin<pro::fader>(x6, c6, skin::globals().fader_time)
                        ->template plugin<pro::notes>(" Hide active window list.               \n"
                                                      " Use mouse wheel to switch it to close. ")
                        ->invoke([&](auto& boss)
                        {
                            boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear)
                            {
                                boss.SIGNAL(tier::general, e2::shutdown, msg, (utf::concat(prompt::desk, "Server shutdown")));
                                gear.dismiss();
                            };
                        });
                    auto drop_bttn = bttn_fork->attach(slot::_2, ui::item::ctor("  ×  ", faux))
                        ->template plugin<pro::fader>(x1, c1, skin::globals().fader_time)
                        ->template plugin<pro::notes>(" Close all open windows in the group ")
                        ->invoke([&](auto& boss)
                        {
                            boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear)
                            {
                                boss.SIGNAL(tier::general, e2::shutdown, msg, (utf::concat(prompt::desk, "Server shutdown")));
                                gear.dismiss();
                            };
                        });
                }
                auto head = head_area->attach(ui::item::ctor(obj_desc, true))
                    ->invoke([&](auto& boss)
                    {
                        auto boss_shadow = ptr::shadow(boss.This());
                        boss.LISTEN(tier::anycast, events::ui::selected, data, -, (inst_id, obj_desc, c9))
                        {
                            auto selected = inst_id == data;
                            boss.set(ansi::fgx(selected ? c9.fgc() : 0x00000000).add(obj_desc));
                            boss.deface();
                        };
                    });
                auto list_pads = block->attach(slot::_2, ui::pads::ctor(dent{ 0,0,0,0 }, dent{ 0,0,0,0 }))
                    ->template plugin<pro::fader>(x3, c3, skin::globals().fader_fast, head_pads);
                auto insts = list_pads->attach(ui::list::ctor())
                    ->attach_collection(e2::form::prop::ui::title, inst_ptr_list, app_template,
                        [&](auto inst_ptr)
                        {
                            auto& window = *inst_ptr;
                            auto& boss = *item_area;
                            boss.LISTEN(tier::release, events::ui::focus::any, gear, window.tracker)
                            {
                                auto deed = boss.bell::template protos<tier::release>();
                                     if (deed == events::ui::focus::set.id) pro::focus::set(window.This(), gear.id, pro::focus::solo::off, pro::focus::flip::off);
                                else if (deed == events::ui::focus::off.id) pro::focus::off(window.This(), gear.id);
                            };
                        });
                insts->invoke([&](auto& boss)
                {
                    auto& group_area = *item_area;
                    boss.LISTEN(tier::release, hids::events::notify::mouse::any, gear, -, (count = 0)) // Turn off group highlighning.
                    {
                        auto deed = boss.bell::template protos<tier::release>();
                        if (deed == hids::events::notify::mouse::leave.id)
                        {
                            if (--count == 0)
                            group_area.SIGNAL(tier::release, e2::form::state::mouse, 1);
                        }
                        else
                        {
                            if (count++ == 0)
                            group_area.SIGNAL(tier::release, e2::form::state::mouse, 0);
                        }
                        //group_area.mouse.take_all_events(true);
                        //group_area.deface();
                    };
                });
            }
            return apps;
        };
        auto background = [](text param)
        {
            auto highlight_color = skin::color(tone::highlight);
            auto c8 = cell{}.bgc(0x00).fgc(highlight_color.bgc());
            auto x8 = cell{ c8 }.alpha(0x00);
            auto ver_label = ui::item::ctor(utf::concat(app::shared::version))
                ->plugin<pro::fader>(x8, c8, 0ms)
                ->alignment({ snap::tail, snap::tail });
            return ui::cake::ctor()
                ->branch(ver_label)
                ->plugin<pro::notes>(" About ")
                ->invoke([&](auto& boss)
                {
                    auto data = utf::divide(param, ";");
                    auto aptype = text{ data.size() > 0 ? data[0] : view{} };
                    auto menuid = text{ data.size() > 1 ? data[1] : view{} };
                    auto params = text{ data.size() > 2 ? data[2] : view{} };
                    boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear, -, (aptype, menuid, params))
                    {
                        static auto offset = dot_00;
                        auto& gate = gear.owner;
                        boss.SIGNAL(tier::anycast, e2::form::prop::viewport, viewport, ());
                        viewport.coor += gear.area().coor;
                        offset = (offset + dot_21 * 2) % (viewport.size * 7 / 32);
                        gear.slot.coor = viewport.coor + offset + viewport.size * 1 / 32;
                        gear.slot.size = viewport.size * 3 / 4;
                        gear.slot_forced = faux;

                        gate.RISEUP(tier::request, desk::events::apps, menu_list_ptr, ());
                        gate.RISEUP(tier::request, desk::events::menu, conf_list_ptr, ());
                        auto& menu_list = *menu_list_ptr;
                        auto& conf_list = *conf_list_ptr;

                        if (conf_list.contains(menuid) && !conf_list[menuid].hidden) // Check for id availability.
                        {
                            auto i = 1;
                            auto testid = text{};
                            do testid = menuid + " (" + std::to_string(++i) + ")";
                            while (conf_list.contains(testid) && !conf_list[menuid].hidden);
                            std::swap(testid, menuid);
                        }
                        auto& m = conf_list[menuid];
                        m.type = aptype;
                        m.label = menuid;
                        m.title = menuid; // Use the same title as the menu label.
                        m.param = params;
                        m.hidden = true;
                        menu_list[menuid];

                        gate.SIGNAL(tier::request, e2::data::changed, lastid, ());
                        gate.SIGNAL(tier::release, e2::data::changed, menuid);
                        gate.RISEUP(tier::request, e2::form::proceed::createby, gear);
                        gate.SIGNAL(tier::release, e2::data::changed, lastid);
                        gear.dismiss();
                    };
                });
        };

        auto build = [](text cwd, text v, xmls& config, text patch)
        {
            auto& tall = skin::globals().menuwide;
            auto highlight_color = skin::globals().highlight;
            auto action_color    = skin::globals().action;
            auto inactive_color  = skin::globals().inactive;
            auto warning_color   = skin::globals().warning;
            auto danger_color    = skin::globals().danger;
            auto cA = inactive_color;
            auto c3 = highlight_color;
            auto x3 = cell{ c3 }.alpha(0x00);
            auto c6 = action_color;
            auto x6 = cell{ c6 }.alpha(0x00);
            auto c2 = warning_color;
            auto x2 = cell{ c2 }.bga(0x00);
            auto c1 = danger_color;
            auto x1 = cell{ c1 }.bga(0x00);

            auto menu_bg_color = config.take("/config/menu/color", cell{}.fgc(whitedk).bgc(0x60202020));
            auto menu_min_size = config.take("/config/menu/width/folded",   si32{ 4  });
            auto menu_max_size = config.take("/config/menu/width/expanded", si32{ 31 });
            auto bttn_min_size = twod{ 31, 1 + tall * 2 };
            auto bttn_max_size = twod{ -1, 1 + tall * 2 };

            auto window = ui::cake::ctor();
            auto my_id = id_t{};

            auto user_info = utf::divide(v, ";");
            if (user_info.size() < 2)
            {
                log(prompt::desk, "Bad window arguments: args=", utf::debase(v));
                return window;
            }
            auto& user_id__view = user_info[0];
            auto& username_view = user_info[1];
            auto& menu_selected = user_info[2];
            log("%%User %name% connected", prompt::desk, username_view);

            if (auto value = utf::to_int(user_id__view)) my_id = value.value();
            else
            {
                log(prompt::desk, "Bad user ID=", user_id__view);
                return window;
            }

            auto client = bell::getref(my_id);
            if (!client)
            {
                log(prompt::desk, "Non-existent user ID=", my_id);
                return window;
            }

            auto user_template = [my_id](auto& data_src, auto const& utf8)
            {
                auto& tall = skin::globals().menuwide;
                auto highlight_color = skin::color(tone::highlight);
                auto c3 = highlight_color;
                auto x3 = cell{ c3 }.alpha(0x00);

                auto item_area = ui::pads::ctor(dent{ 1,0,0,tall }, dent{ 0,0,tall,0 })
                    ->plugin<pro::fader>(x3, c3, skin::globals().fader_time)
                    ->plugin<pro::notes>(" Connected user ");
                auto user = item_area->attach(ui::item::ctor(escx(" &").nil().add(" ")
                    //.link(data_src->id).fgx(data_src->id == my_id ? rgba::vt256[whitelt] : 0x00).add(utf8).nil(), true));
                    .fgx(data_src->id == my_id ? rgba::vt256[whitelt] : 0x00).add(utf8).nil(), true));
                return item_area;
            };
            auto branch_template = [user_template](auto& data_src, auto& usr_list)
            {
                auto users = ui::list::ctor()
                    ->attach_collection(e2::form::prop::name, *usr_list, user_template);
                return users;
            };

            window->invoke([menu_max_size, menu_min_size, menu_selected](auto& boss) mutable
            {
                auto ground = background("gems;About;");
                auto current_default  = text{ menu_selected };
                auto previous_default = text{ menu_selected };
                boss.LISTEN(tier::release, e2::form::upon::vtree::attached, parent, -, (ground, current_default, previous_default, tokens = subs{}))
                {
                    ground->SIGNAL(tier::release, e2::form::upon::vtree::attached, parent);
                    parent->SIGNAL(tier::anycast, events::ui::selected, current_default);
                    parent->LISTEN(tier::request, e2::data::changed, data, tokens)
                    {
                        data = current_default;
                    };
                    parent->LISTEN(tier::preview, e2::data::changed, data, tokens)
                    {
                        data = previous_default;
                    };
                    parent->LISTEN(tier::release, e2::data::changed, data, tokens)
                    {
                        boss.SIGNAL(tier::anycast, events::ui::selected, data);
                    };
                    parent->LISTEN(tier::anycast, events::ui::selected, data, tokens)
                    {
                        auto new_default = data;
                        if (current_default != new_default)
                        {
                            previous_default = current_default;
                            current_default = new_default;
                        }
                    };
                    parent->LISTEN(tier::release, e2::form::upon::vtree::detached, p, tokens)
                    {
                        current_default.clear();
                        previous_default.clear();
                        tokens.clear();
                    };
                    parent->LISTEN(tier::release, e2::size::any, new_size, tokens)
                    {
                        ground->base::resize(new_size);
                    };
                    parent->LISTEN(tier::release, e2::render::prerender, parent_canvas, tokens, (parent_id = parent->id))
                    {
                        if (parent_id == parent_canvas.mark().link())
                        {
                            ground->render(parent_canvas);
                        }
                    };
                };
            });
            auto taskbar_viewport = window->attach(ui::fork::ctor(axis::X))
                ->invoke([](auto& boss)
                {
                    boss.LISTEN(tier::anycast, e2::form::prop::viewport, viewport)
                    {
                        viewport = boss.base::area();
                    };
                });
            auto taskbar_park = taskbar_viewport->attach(slot::_1, ui::cake::ctor())
                ->colors(menu_bg_color.fgc(), menu_bg_color.bgc())
                ->limits({ menu_min_size,-1 }, { menu_min_size,-1 })
                ->plugin<pro::notes>(" LeftDrag to adjust the taskbar width                        \n"
                                     " Ctrl+LeftDrag to adjust the folded taskbar width            \n"
                                     " RightDrag or scroll wheel to slide the taskbar menu up/down ")
                ->plugin<pro::timer>()
                ->plugin<pro::acryl>()
                ->plugin<pro::cache>()
                ->invoke([&](auto& boss)
                {
                    boss.mouse.template draggable<hids::buttons::left>(true);
                    auto size_config = ptr::shared(std::tuple{ menu_max_size, menu_min_size, faux, datetime::now() });
                    auto toggle = [&, size_config](bool state)
                    {
                        auto& [menu_max_size, menu_min_size, active, skip] = *size_config;
                        active = state;
                        auto size = active ? std::max(menu_max_size, menu_min_size)
                                           : menu_min_size;
                        auto lims = twod{ size,-1 };
                        boss.base::limits(lims, lims);
                        boss.base::reflow();
                    };
                    boss.LISTEN(tier::release, e2::form::drag::pull::_<hids::buttons::left>, gear, -, (size_config))
                    {
                        auto& [menu_max_size, menu_min_size, active, skip] = *size_config;
                        boss.minlim.x += gear.delta.get().x;
                        boss.maxlim.x = boss.minlim.x;
                        gear.meta(hids::anyCtrl) ? menu_min_size = boss.minlim.x
                                                 : menu_max_size = boss.minlim.x;
                        boss.base::reflow();
                    };
                    //todo rewrite taskbar
                    //boss.LISTEN(tier::preview, hids::events::keybd::data::any, gear, -, (toggle))
                    //{
                    //    auto& [menu_max_size, menu_min_size, active] = *size_config;
                    //    toggle(!active);
                    //};
                    //boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear, -, (toggle))
                    //{
                    //    auto& [menu_max_size, menu_min_size, active] = *size_config;
                    //    toggle(!active);
                    //};
                    boss.LISTEN(tier::anycast, e2::form::layout::restore, item_ptr, -, (size_config))
                    {
                        auto& [menu_max_size, menu_min_size, active, skip] = *size_config;
                        skip = datetime::now();
                    };
                    boss.LISTEN(tier::release, e2::form::state::mouse, state, -, (size_config))
                    {
                        auto& [menu_max_size, menu_min_size, active, skip] = *size_config;
                        //todo too hacky
                        if (state && skip + 500ms > datetime::now())
                        {
                            return;
                        }
                        auto apply = [&, size_config](auto state)
                        {
                            auto& [menu_max_size, menu_min_size, active, skip] = *size_config;
                            active = state;
                            auto size = active ? std::max(menu_max_size, menu_min_size)
                                               : menu_min_size;
                            auto lims = twod{ size,-1 };
                            boss.base::limits(lims, lims);
                            boss.base::reflow();
                            return faux; // One shot call.
                        };
                        auto& timer = boss.template plugins<pro::timer>();
                        timer.pacify(faux);
                        if (state) apply(true);
                        else       timer.actify(faux, skin::globals().menu_timeout, apply);
                    };
                    boss.LISTEN(tier::anycast, e2::form::prop::viewport, viewport, -, (size_config))
                    {
                        auto& [menu_max_size, menu_min_size, active, skip] = *size_config;
                        viewport.coor.x += menu_min_size;
                        viewport.size.x -= menu_min_size;
                    };
                });
            auto taskbar = taskbar_park->attach(ui::fork::ctor(axis::Y)->alignment({ snap::head, snap::head }, { snap::head, snap::tail }));
            auto apps_users = taskbar->attach(slot::_1, ui::fork::ctor(axis::Y, 0, 100));
            auto applist_area = apps_users->attach(slot::_1, ui::pads::ctor(dent{ 0,0,tall,0 }, dent{}))
                ->attach(ui::cake::ctor());
            auto tasks_scrl = applist_area->attach(ui::rail::ctor(axes::Y_only))
                ->colors(0x00, 0x00) //todo mouse events passthrough
                ->invoke([&](auto& boss)
                {
                    boss.LISTEN(tier::anycast, e2::form::upon::started, parent_ptr)
                    {
                        boss.RISEUP(tier::request, e2::config::creator, world_ptr, ());
                        if (world_ptr)
                        {
                            auto apps = boss.attach_element(desk::events::apps, world_ptr, apps_template);
                        }
                    };
                });
            auto users_area = apps_users->attach(slot::_2, ui::fork::ctor(axis::Y));
            auto label_pads = users_area->attach(slot::_1, ui::pads::ctor(dent{ 0,0,tall,tall }, dent{ 0,0,0,0 }))
                                        ->plugin<pro::notes>(" List of connected users ");
            auto label_bttn = label_pads->attach(ui::fork::ctor(axis::X));//, 0, 1, 1, true));
            auto label = label_bttn->attach(slot::_1, ui::item::ctor("users", true, faux, true))
                ->limits({ 5,-1 })
                ->colors(cA.fgc(), cA.bgc());
            auto bttn_pads = label_bttn->attach(slot::_2, ui::pads::ctor(dent{ 2,2,0,0 }, dent{ 0,0,tall,tall }))
                ->plugin<pro::fader>(x6, c6, skin::globals().fader_time)
                ->plugin<pro::notes>(" Show/hide user list ");
            auto userlist_hidden = true;
            auto bttn = bttn_pads->attach(ui::item::ctor(userlist_hidden ? "…" : "<", faux));
            auto userlist_area = users_area->attach(slot::_2, ui::pads::ctor())
                ->invoke([&](auto& boss)
                {
                    boss.hidden = userlist_hidden;
                    boss.LISTEN(tier::anycast, e2::form::upon::started, parent_ptr, -, (branch_template))
                    {
                        boss.RISEUP(tier::request, e2::config::creator, world_ptr, ());
                        if (world_ptr)
                        {
                            auto users = boss.attach_element(desk::events::usrs, world_ptr, branch_template);
                        }
                    };
                });
            bttn_pads->invoke([&](auto& boss)
            {
                auto userlist_area_shadow = ptr::shadow(userlist_area);
                auto bttn_shadow = ptr::shadow(bttn);
                boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear, -, (userlist_area_shadow, bttn_shadow))
                {
                    if (auto bttn = bttn_shadow.lock())
                    if (auto userlist = userlist_area_shadow.lock())
                    {
                        auto& hidden = userlist->base::hidden;
                        hidden = !hidden;
                        bttn->set(hidden ? "…" : "<");
                        userlist->base::reflow();
                    }
                };
            });
            auto bttns_cake = taskbar->attach(slot::_2, ui::cake::ctor());
            auto bttns_area = bttns_cake->attach(ui::rail::ctor(axes::X_only))
                ->limits({ -1, 1 + tall * 2 }, { -1, 1 + tall * 2 });
            bttns_cake->attach(app::shared::underlined_hz_scrollbars(bttns_area));
            auto bttns = bttns_area->attach(ui::fork::ctor(axis::X))
                ->limits(bttn_min_size, bttn_max_size);
            auto disconnect_park = bttns->attach(slot::_1, ui::cake::ctor())
                ->plugin<pro::fader>(x2, c2, skin::globals().fader_time)
                ->plugin<pro::notes>(" Leave current session ")
                ->invoke([&, name = text{ username_view }](auto& boss)
                {
                    boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear, -, (name))
                    {
                        log("%%User %name% disconnected", prompt::desk, name);
                        gear.owner.SIGNAL(tier::preview, e2::conio::quit, deal, ());
                        gear.dismiss();
                    };
                });
            auto disconnect_area = disconnect_park->attach(ui::pads::ctor(dent{ 2,3,tall,tall })->alignment({ snap::head, snap::center }));
            auto disconnect = disconnect_area->attach(ui::item::ctor("× Disconnect"));
            auto shutdown_park = bttns->attach(slot::_2, ui::cake::ctor())
                ->plugin<pro::fader>(x1, c1, skin::globals().fader_time)
                ->plugin<pro::notes>(" Disconnect all users and shutdown the server ")
                ->invoke([&](auto& boss)
                {
                    boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear)
                    {
                        boss.SIGNAL(tier::general, e2::shutdown, msg, (utf::concat(prompt::desk, "Server shutdown")));
                    };
                });
            auto shutdown_area = shutdown_park->attach(ui::pads::ctor(dent{ 2,3,tall,tall })->alignment({ snap::tail, snap::center }));
            auto shutdown = shutdown_area->attach(ui::item::ctor("× Shutdown"));
            return window;
        };
    }

    app::shared::initialize builder{ app::desk::id, build };
}