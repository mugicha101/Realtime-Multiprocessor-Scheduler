#include "model.h"
#include "view.h"
#include "schedulers.h"
#include <iostream>
#include <cmath>

const float MAX_ZOOM = 50.f;
const float MIN_ZOOM = 0.5f;
const float START_ZOOM = 2.0f;

int main() {
    View::init();
    Model model;
    // setup test model
    TaskSet tset;
    tset.push_back(Task(20, 15));
    tset.push_back(Task(10, 5));
    tset.push_back(Task(20, 8));
    tset.push_back(Task(10, 8));
    tset.push_back(Task(20, 11));
    Scheduler* scheduler = new PD2(true);
    scheduler->init(tset);
    model.sim.reset(tset, scheduler, 3);

    View view;
    view.tf = Transform::scale(START_ZOOM) * Transform::id();
    const float init_scale = 0.8f;
    view.open(sf::VideoMode::getDesktopMode().width * init_scale, sf::VideoMode::getDesktopMode().height * init_scale);
    MouseState mouse;
    mouse.mouse_down = false;
    mouse.mouse_lost = true;
    while (view.window.isOpen()) {
        Pos new_mouse_pos = mouse.screen_pos;
        bool mouse_moved = false;
        int zoom_action = 0.f;

        // input step - handle events
        for (auto event = sf::Event{}; view.window.pollEvent(event);) {
            switch (event.type) {
                case sf::Event::Closed:
                    view.close();
                    break;
                case sf::Event::MouseWheelMoved:
                    if (event.mouseWheel.delta > 0) {
                        zoom_action = 1;
                    } else if (event.mouseWheel.delta < 0) {
                        zoom_action = -1;
                    }
                    break;
                case sf::Event::LostFocus:
                case sf::Event::MouseLeft:
                    mouse.mouse_down = false;
                case sf::Event::GainedFocus:
                    mouse.mouse_lost = true;
                    break;
                case sf::Event::MouseButtonPressed:
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        mouse.mouse_down = true;
                        mouse.mouse_lost = true;
                    }
                    break;
                case sf::Event::MouseButtonReleased:
                    if (event.mouseButton.button == sf::Mouse::Left) mouse.mouse_down = false;
                    break;
                case sf::Event::MouseMoved:
                    mouse_moved = true;
                    new_mouse_pos.x = event.mouseMove.x;
                    new_mouse_pos.y = event.mouseMove.y;
                    break;
                case sf::Event::KeyPressed:
                    switch (event.key.code) {
                        case sf::Keyboard::Right:
                            if (view.block_stretch < (1 << 20))
                                view.block_stretch <<= 1;
                            break;
                        case sf::Keyboard::Left:
                            if (view.block_stretch > 1)
                                view.block_stretch >>= 1;
                    }
            }
        }

        // handle mouse actions
        if (mouse_moved) {
            if (mouse.mouse_lost) {
                mouse.mouse_lost = false;
                mouse.screen_pos = new_mouse_pos;
            }
            Pos mouse_pos_change = new_mouse_pos - mouse.screen_pos;
            if (mouse.mouse_down) {
                view.tf = Transform::trans(mouse_pos_change.x, mouse_pos_change.y) * view.tf;
            }
            mouse.screen_pos = new_mouse_pos;
        }
        if (zoom_action) {
            float zoom_factor = zoom_action == 1 ? 1.25f : 0.8f;
            if (view.tf.sy() * zoom_factor <= MAX_ZOOM && view.tf.sy() * zoom_factor >= MIN_ZOOM) {
                Pos sim_pos = mouse.screen_pos;
                view.tf = Transform::trans(sim_pos.x, sim_pos.y) * Transform::scale(zoom_factor) * Transform::trans(-sim_pos.x, -sim_pos.y) * view.tf;
            }
        }
        mouse.sim_pos = view.tf.inv() * mouse.screen_pos;

        // calc step - update model
        int end_time = (int)std::ceil((view.tf.inv() * Pos(view.window.getSize())).x);
        model.sim.sim(end_time);

        // draw step - update view
        view.update(model);
    }
}