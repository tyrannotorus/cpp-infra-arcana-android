// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CONFIG_OPTIONS_HPP
#define CONFIG_OPTIONS_HPP

#include <string>

namespace config
{
// Values for user changing option left/right or pressing enter.
enum class OptionChangeCommand
{
        enter,
        left,
        right
};

class Option
{
public:
        virtual void change(OptionChangeCommand command) const = 0;

        virtual std::string name() const = 0;

        virtual std::string value_str() const = 0;

        // Some options play a custom selection audio, so they must disable the
        // one played by the menu browser.
        virtual bool allow_browser_selection_audio() const
        {
                return true;
        }
};

class MasterVolumeOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;

        bool allow_browser_selection_audio() const override
        {
                return false;
        }
};

class AmbientAudioEnabledOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;

        bool allow_browser_selection_audio() const override
        {
                return false;
        }
};

class PreloadAmbientAudioOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class InputModeOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class AlwaysCenterViewOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class TilesModeOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class FontOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class FullscreenOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class VideoScalingOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class TextModeFilledWallsOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class SkipIntroLevelOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class SkipIntroPopupOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class AnyKeyConfirmMoreOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class DisplayHintsOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class AlwaysWarnMonsterOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class WarnThrowValuableOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class WarnLightExplosivesOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class WanDrinkMalignPotionOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class WarnRangedWeaponMeleeOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class AutoReloadOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class ProjectileDelayOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class ShotgunDelayOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class ExplosionDelayOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

class ResetDefaultsOption : public Option
{
public:
        void change(OptionChangeCommand command) const override;

        std::string name() const override;

        std::string value_str() const override;
};

}  // namespace config

#endif  // CONFIG_HPP
