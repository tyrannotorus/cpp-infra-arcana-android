// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CONFIG_OPTIONS_HPP
#define CONFIG_OPTIONS_HPP

#include <string>

namespace config
{
enum class OptionSubmenuType
{
        video,
        audio,
        input,
        gameplay,
        END
};

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
        virtual ~Option() = default;
        virtual std::string name() const = 0;
        virtual std::string descr() const = 0;
        virtual std::string value_str() const = 0;
        virtual OptionSubmenuType submenu_type() const = 0;
        virtual void change(OptionChangeCommand command) const = 0;

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
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;

        bool allow_browser_selection_audio() const override
        {
                return false;
        }
};

class AmbientAudioEnabledOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;

        bool allow_browser_selection_audio() const override
        {
                return false;
        }
};

class PreloadAmbientAudioOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class AudioBufferSizeOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;

        bool allow_browser_selection_audio() const override
        {
                return false;
        }
};

class InputModeOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class DoubleClickTogglesFullscreenOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class AlwaysCenterViewOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class TilesModeOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class FontOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class RendererTypeOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class FullscreenOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class VideoScaleOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class GameScaleOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class BrightnessOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class TextModeFilledWallsOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class DisplayHealthBarsOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class UseTrapColorWhenObscuredOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class SkipIntroLevelOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class SkipIntroPopupOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class AnyKeyConfirmMoreOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class AutoSelectMenuOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class DisplayHintsOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class AlwaysWarnMonsterOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class WarnThrowValuableOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class WarnLightExplosivesOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class WanDrinkMalignPotionOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class WarnRangedWeaponMeleeOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class MedicalBagAutoChoiceOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class AutoReloadOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class ProjectileDelayOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

class ExplosionDelayOption : public Option
{
public:
        std::string name() const override;
        std::string descr() const override;
        std::string value_str() const override;
        OptionSubmenuType submenu_type() const override;
        void change(OptionChangeCommand command) const override;
};

// class ResetDefaultsOption : public Option
// {
// public:
//         std::string name() const override;

//         std::string descr() const override;

//         std::string value_str() const override;

//         OptionSubmenuType submenu_type() const override;

//         void change(OptionChangeCommand command) const override;
// };

}  // namespace config

#endif  // CONFIG_OPTIONS_HPP
