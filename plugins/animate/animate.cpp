#include <output.hpp>
#include <signal-definitions.hpp>
#include <render-manager.hpp>
#include <workspace-manager.hpp>
#include <debug.hpp>
#include <type_traits>
#include <core.hpp>
#include "system_fade.hpp"
#include "basic_animations.hpp"
#include "fire/fire.hpp"

#include "../matcher/matcher.hpp"

void animation_base::init(wayfire_view, wf_option, wf_animation_type) {}
bool animation_base::step() {return false;}
animation_base::~animation_base() {}

/* Represents an animation running for a specific view
 * animation_t is which animation to use (i.e fire, zoom, etc). */
template<class animation_t>
struct animation_hook : public wf_custom_data_t
{
    static_assert(std::is_base_of<animation_base, animation_t>::value,
            "animation_type must be derived from animation_base!");

    static constexpr const char* custom_data_id = "animation-hook";

    wf_animation_type type;
    std::unique_ptr<animation_base> animation;

    wayfire_view view;
    wf::output_t *output;

    /* Update animation right before each frame */
    wf::effect_hook_t update_animation_hook = [=] ()
    {
        view->damage();
        bool result = animation->step();
        view->damage();

        if (!result)
            stop_hook(false);
    };

    /* If the view changes outputs, we need to stop animating, because our animations,
     * hooks, etc are bound to the last output. */
    signal_callback_t view_detached = [=] (signal_data *data)
    {
        if (get_signaled_view(data) == view)
            stop_hook(true);
    };

    animation_hook(wayfire_view view, wf_option duration, wf_animation_type type)
    {
        this->type = type;
        this->view = view;
        this->output = view->get_output();

        if (type == ANIMATION_TYPE_UNMAP)
        {
            view->inc_keep_count();
            view->take_snapshot();
        }

        animation = std::make_unique<animation_t> ();
        animation->init(view, duration, type);

        output->render->add_effect(&update_animation_hook, wf::OUTPUT_EFFECT_PRE);

        /* We listen for just the detach-view signal. If the state changes in
         * some other way (i.e view unmapped while map animation), the hook
         * will be overridden by wayfire_animation::set_animation() */
        output->connect_signal("detach-view", &view_detached);
    }

    void stop_hook(bool detached)
    {
        /* We don't want to change the state of the view if it was detached */
        if (type == ANIMATION_TYPE_MINIMIZE && !detached)
            view->set_minimized(true);

        /* Special case: we are animating view unmap, and we are the "last"
         * who have a keep count on it. In this case, we can just descrease keep_count,
         * which will destroy the view and ourselves */
        if (view->keep_count == 1 && type == ANIMATION_TYPE_UNMAP)
            return view->dec_keep_count();

        /* Will also delete this */
        view->erase_data(custom_data_id);
    }

    ~animation_hook()
    {
        /* We do not want to decrease keep_count twice, see the special case
         * above. */
        if (type == ANIMATION_TYPE_UNMAP && view->keep_count > 0)
            view->dec_keep_count();

        output->render->rem_effect(&update_animation_hook);
        output->disconnect_signal("detach-view", &view_detached);
    }
};

class wayfire_animation : public wayfire_plugin_t
{
    wf_option open_animation, close_animation;
    wf_option duration, startup_duration;
    wf_option animation_enabled_for, fade_enabled_for,
              zoom_enabled_for, fire_enabled_for;

    std::unique_ptr<wf::matcher::view_matcher> animation_enabled_matcher,
        fade_enabled_matcher, zoom_enabled_matcher, fire_enabled_matcher;

    public:
    void init(wayfire_config *config)
    {
        grab_interface->name = "animate";
        grab_interface->abilities_mask = WF_ABILITY_CUSTOM_RENDERING;

        auto section     = config->get_section("animate");
        open_animation   = section->get_option("open_animation", "fade");
        close_animation  = section->get_option("close_animation", "fade");
        duration         = section->get_option("duration", "300");
        startup_duration = section->get_option("startup_duration", "600");

        animation_enabled_for = section->get_option("enabled_for",
            "(type is toplevel || (type is x-or && focuseable is true))");
        fade_enabled_for = section->get_option("fade_enabled_for", "type is overlay");
        zoom_enabled_for = section->get_option("zoom_enabled_for", "none");
        fire_enabled_for = section->get_option("fire_enabled_for", "none");

        FireAnimation::fire_particles =
            section->get_option("fire_particles", "2000");
        FireAnimation::fire_particle_size =
            section->get_option("fire_particle_size", "16");

        output->connect_signal("map-view", &on_view_mapped);
        output->connect_signal("unmap-view", &on_view_unmapped);
        output->connect_signal("start-rendering", &on_render_start);
        output->connect_signal("view-minimize-request", &on_minimize_request);

        animation_enabled_matcher =
            wf::matcher::get_matcher(*core, animation_enabled_for);
        fade_enabled_matcher = wf::matcher::get_matcher(*core, fade_enabled_for);
        zoom_enabled_matcher = wf::matcher::get_matcher(*core, zoom_enabled_for);
        fire_enabled_matcher = wf::matcher::get_matcher(*core, fire_enabled_for);
    }

    std::string get_animation_for_view(wf_option& anim_type, wayfire_view view)
    {
        /* Determine the animation for the given view.
         * Note that the matcher plugin might not have been loaded, so
         * we need to have a fallback algorithm */
        if (animation_enabled_matcher)
        {
            if (WF_MATCHER_MATCHES(fade_enabled_matcher, view))
                return "fade";
            if (WF_MATCHER_MATCHES(zoom_enabled_matcher, view))
                return "zoom";
            if (WF_MATCHER_MATCHES(fire_enabled_matcher, view))
                return "fire";
            if (WF_MATCHER_MATCHES(animation_enabled_matcher, view))
                return anim_type->as_string();
        }
        else if (view->role == WF_VIEW_ROLE_TOPLEVEL ||
            (view->role == WF_VIEW_ROLE_UNMANAGED && view->is_focuseable()))
        {
            return anim_type->as_string();
        }

        return "none";
    }

    template<class animation_t>
        void set_animation(wayfire_view view, wf_animation_type type)
    {
        view->store_data(
            std::make_unique<animation_hook<animation_t>> (view, duration, type),
            animation_hook<animation_t>::custom_data_id);
    }

    /* TODO: enhance - add more animations */
    signal_callback_t on_view_mapped = [=] (signal_data *ddata) -> void
    {
        auto view = get_signaled_view(ddata);
        auto animation = get_animation_for_view(open_animation, view);

        if (animation == "fade")
            set_animation<fade_animation> (view, ANIMATION_TYPE_MAP);
        else if (animation == "zoom")
            set_animation<zoom_animation> (view, ANIMATION_TYPE_MAP);
        else if (animation == "fire")
            set_animation<FireAnimation> (view, ANIMATION_TYPE_MAP);
    };

    signal_callback_t on_view_unmapped = [=] (signal_data *data) -> void
    {
        auto view = get_signaled_view(data);
        auto animation = get_animation_for_view(close_animation, view);

        if (animation == "fade")
            set_animation<fade_animation> (view, ANIMATION_TYPE_UNMAP);
        else if (animation == "zoom")
            set_animation<zoom_animation> (view, ANIMATION_TYPE_UNMAP);
        else if (animation == "fire")
            set_animation<FireAnimation> (view, ANIMATION_TYPE_UNMAP);
    };

    signal_callback_t on_minimize_request = [=] (signal_data *data) -> void
    {
        auto ev = static_cast<view_minimize_request_signal*> (data);
        if (ev->state) {
            ev->carried_out = true;
            set_animation<zoom_animation>(ev->view, ANIMATION_TYPE_MINIMIZE);
        } else {
            set_animation<zoom_animation> (ev->view, ANIMATION_TYPE_RESTORE);
        }
    };

    signal_callback_t on_render_start = [=] (signal_data *data) -> void
    {
        new wf_system_fade(output, startup_duration);
    };

    void fini()
    {
        output->disconnect_signal("map-view", &on_view_mapped);
        output->disconnect_signal("unmap-view", &on_view_unmapped);
        output->disconnect_signal("start-rendering", &on_render_start);
        output->disconnect_signal("view-minimize-request", &on_minimize_request);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_animation();
    }
}
