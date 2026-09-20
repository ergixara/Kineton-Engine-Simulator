#include <gtest/gtest.h>
#include <delta-studio/include/yds_mouse_aggregator.h>
#include <delta-studio/include/yds_keyboard_aggregator.h>

TEST(InputBackend, QuickClickRetainsBothEdgesAndReleasePosition) {
    ysMouse mouse;
    mouse.SetOsPosition(40, 50);
    mouse.UpdateButton(ysMouse::Button::Left, ysMouse::ButtonState::DownTransition);
    mouse.SetOsPosition(41, 51);
    mouse.UpdateButton(ysMouse::Button::Left, ysMouse::ButtonState::UpTransition);
    EXPECT_FALSE(mouse.IsDown(ysMouse::Button::Left));
    ysMouse::ButtonEvent event;
    ASSERT_TRUE(mouse.PollButtonEvent(&event));
    EXPECT_EQ(event.state, ysMouse::ButtonState::DownTransition);
    EXPECT_EQ(event.x, 40);
    ASSERT_TRUE(mouse.PollButtonEvent(&event));
    EXPECT_EQ(event.state, ysMouse::ButtonState::UpTransition);
    EXPECT_EQ(event.x, 41);
    EXPECT_FALSE(mouse.PollButtonEvent(&event));
}

TEST(InputBackend, MultipleDevicesPreserveEventOrderAndDragExcursion) {
    ysMouse first, second;
    ysMouseAggregator all;
    all.RegisterMouse(&first);
    all.RegisterMouse(&second);
    second.SetOsPosition(30, 40);
    second.UpdateButton(ysMouse::Button::Left, ysMouse::ButtonState::DownTransition);
    first.SetOsPosition(90, 40);
    first.QueueMoveEvent();
    second.QueueMoveEvent();
    second.UpdateButton(ysMouse::Button::Left, ysMouse::ButtonState::UpTransition);
    ysMouse::ButtonEvent event;
    ASSERT_TRUE(all.PollButtonEvent(&event));
    EXPECT_EQ(event.state, ysMouse::ButtonState::DownTransition);
    ASSERT_TRUE(all.PollButtonEvent(&event));
    EXPECT_EQ(event.button, ysMouse::Button::Count);
    EXPECT_EQ(event.x, 90);
    ASSERT_TRUE(all.PollButtonEvent(&event));
    EXPECT_EQ(event.button, ysMouse::Button::Count);
    EXPECT_EQ(event.x, 30);
    ASSERT_TRUE(all.PollButtonEvent(&event));
    EXPECT_EQ(event.state, ysMouse::ButtonState::UpTransition);
    EXPECT_FALSE(all.PollButtonEvent(&event));
}

TEST(InputBackend, CancelClearsEdgesAndHoldsWithoutChangingWheel) {
    ysMouse mouse;
    mouse.UpdateWheel(240);
    mouse.UpdateButton(ysMouse::Button::Right, ysMouse::ButtonState::DownTransition);
    mouse.ClearButtons();
    EXPECT_FALSE(mouse.IsDown(ysMouse::Button::Right));
    EXPECT_EQ(mouse.GetWheel(), 240);
    ysMouse::ButtonEvent event;
    EXPECT_FALSE(mouse.PollButtonEvent(&event));
}

TEST(InputBackend, KeyboardFastTapSurvivesReleaseAndRepeatDoesNotRetoggle) {
    ysKeyboard keyboard;
    keyboard.SetKeyState(ysKey::Code::F6, ysKey::State::DownTransition, ysKey::Variation::Undefined);
    keyboard.SetKeyState(ysKey::Code::F6, ysKey::State::DownTransition, ysKey::Variation::Undefined);
    keyboard.SetKeyState(ysKey::Code::F6, ysKey::State::UpTransition, ysKey::Variation::Undefined);
    EXPECT_FALSE(keyboard.IsKeyDown(ysKey::Code::F6));
    EXPECT_TRUE(keyboard.ProcessKeyTransition(ysKey::Code::F6));
    EXPECT_FALSE(keyboard.ProcessKeyTransition(ysKey::Code::F6));
    EXPECT_TRUE(keyboard.ProcessKeyTransition(ysKey::Code::F6, ysKey::State::UpTransition));
    keyboard.SetKeyState(ysKey::Code::F7, ysKey::State::DownTransition, ysKey::Variation::Undefined);
    keyboard.ClearKeyStates();
    EXPECT_FALSE(keyboard.IsKeyDown(ysKey::Code::F7));
    EXPECT_FALSE(keyboard.ProcessKeyTransition(ysKey::Code::F7));
}

TEST(InputBackend, AggregatorConsumesSimultaneousKeyboardEdgesOnce) {
    ysKeyboard first, second;
    ysKeyboardAggregator all;
    all.RegisterKeyboard(&first);
    all.RegisterKeyboard(&second);
    first.SetKeyState(ysKey::Code::F8, ysKey::State::DownTransition, ysKey::Variation::Undefined);
    second.SetKeyState(ysKey::Code::F8, ysKey::State::DownTransition, ysKey::Variation::Undefined);
    EXPECT_TRUE(all.ProcessKeyTransition(ysKey::Code::F8));
    EXPECT_FALSE(all.ProcessKeyTransition(ysKey::Code::F8));
}
