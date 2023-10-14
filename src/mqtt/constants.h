#pragma once

namespace mqtt
{

namespace prop
{
constexpr static const char *STATE = "state";
constexpr static const char *BRIGHTNESS = "brightness";
} // namespace prop

namespace state
{
constexpr static const char *ON = "ON";
constexpr static const char *OFF = "OFF";
} // namespace state

namespace availability
{
constexpr static const char *ONLINE = "ONLINE";
constexpr static const char *OFFLINE = "OFFLINE";
} // namespace availability

namespace brightness
{
constexpr static const int MIN = 0;
constexpr static const int MAX = 255;
} // namespace brightness

} // namespace mqtt