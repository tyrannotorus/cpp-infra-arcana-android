// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef INSANITY_HPP
#define INSANITY_HPP

#include <string>
#include <vector>

namespace actor
{
class Actor;
}  // namespace actor

enum class InsSymptId
{
        reduce_xp,
        scream,
        babbling,
        faint,
        laugh,
        phobia_rat,
        phobia_spider,
        phobia_reptile_and_amph,
        phobia_canine,
        phobia_dead,
        phobia_deep,
        phobia_dark,
        sadism,
        shadows,
        paranoia,  // Invisible stalker spawned
        confusion,
        frenzy,
        strange_sensation,
        END
};

enum class InsSymptType
{
        phobia,
        misc
};

class InsSympt
{
public:
        InsSympt() = default;

        virtual ~InsSympt() = default;

        virtual InsSymptId id() const = 0;

        virtual InsSymptType type() const = 0;

        virtual void save() const {}

        virtual void load() {}

        virtual bool is_permanent() const = 0;

        virtual bool is_allowed() const
        {
                return true;
        }

        void on_start();

        void on_end();

        virtual void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes)
        {
                (void)seen_foes;
        }

        virtual void on_permanent_rfear() {}

        virtual std::string char_descr_msg() const
        {
                return "";
        }

        virtual std::string postmortem_msg() const
        {
                return "";
        }

protected:
        virtual void on_start_hook() {}

        virtual std::string start_msg() const = 0;
        virtual std::string start_heading() const = 0;

        virtual std::string end_msg() const
        {
                return "";
        }

        virtual std::string history_msg() const = 0;

        virtual std::string history_msg_end() const
        {
                return "";
        }
};

class InsReduceXp : public InsSympt
{
public:
        InsReduceXp() = default;

        InsSymptId id() const override
        {
                return InsSymptId::reduce_xp;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override;

        std::string start_heading() const override
        {
                return "Experiences erased!";
        }

        std::string history_msg() const override
        {
                return "Experiences were erased from memory";
        }
};

class InsScream : public InsSympt
{
public:
        InsScream() = default;

        InsSymptId id() const override
        {
                return InsSymptId::scream;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override;

        std::string start_heading() const override
        {
                return "Screaming!";
        }

        std::string history_msg() const override
        {
                return "Screamed in terror";
        }
};

class InsBabbling : public InsSympt
{
public:
        InsBabbling() = default;

        InsSymptId id() const override
        {
                return InsSymptId::babbling;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes) override;

        std::string char_descr_msg() const override
        {
                return "Babbling";
        }

        std::string postmortem_msg() const override
        {
                return "Had a tendency to babble";
        }

        void babble() const;

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "I find myself babbling incoherently.";
        }

        std::string start_heading() const override
        {
                return "Babbling!";
        }

        std::string end_msg() const override
        {
                return "I feel in control of my speech.";
        }

        std::string history_msg() const override
        {
                return "Started babbling incoherently";
        }

        std::string history_msg_end() const override
        {
                return "My strange babbling was cured";
        }
};

class InsFaint : public InsSympt
{
public:
        InsFaint() = default;

        InsSymptId id() const override
        {
                return InsSymptId::faint;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "Everything is blacking out.";
        }

        std::string start_heading() const override
        {
                return "Fainting!";
        }

        std::string history_msg() const override
        {
                return "Fainted";
        }
};

class InsLaugh : public InsSympt
{
public:
        InsLaugh() = default;

        InsSymptId id() const override
        {
                return InsSymptId::laugh;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "I laugh maniacally.";
        }

        std::string start_heading() const override
        {
                return "HAHAHA!";
        }

        std::string history_msg() const override
        {
                return "Laughed maniacally";
        }
};

class InsPhobiaRat : public InsSympt
{
public:
        InsPhobiaRat() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_rat;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "Phobia of rats";
        }

        std::string postmortem_msg() const override
        {
                return "Had a phobia of rats";
        }

protected:
        std::string start_msg() const override
        {
                return "Rats suddenly seem terrifying.";
        }

        std::string start_heading() const override
        {
                return "Murophobia!";
        }

        std::string end_msg() const override
        {
                return "I am no longer terrified of rats.";
        }

        std::string history_msg() const override
        {
                return "Gained a phobia of rats";
        }

        std::string history_msg_end() const override
        {
                return "My phobia of rats was cured";
        }
};

class InsPhobiaSpider : public InsSympt
{
public:
        InsPhobiaSpider() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_spider;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "Phobia of spiders";
        }

        std::string postmortem_msg() const override
        {
                return "Had a phobia of spiders";
        }

protected:
        std::string start_msg() const override
        {
                return "Spiders suddenly seem terrifying.";
        }

        std::string start_heading() const override
        {
                return "Arachnophobia!";
        }

        std::string end_msg() const override
        {
                return "I am no longer terrified of spiders.";
        }

        std::string history_msg() const override
        {
                return "Gained a phobia of spiders";
        }

        std::string history_msg_end() const override
        {
                return "My phobia of spiders was cured";
        }
};

class InsPhobiaReptileAndAmph : public InsSympt
{
public:
        InsPhobiaReptileAndAmph() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_reptile_and_amph;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "Phobia of reptiles and amphibians";
        }

        std::string postmortem_msg() const override
        {
                return "Had a phobia of reptiles and amphibians";
        }

protected:
        std::string start_msg() const override
        {
                return "Reptiles and amphibians suddenly seem terrifying.";
        }

        std::string start_heading() const override
        {
                return "Herpetophobia!";
        }

        std::string end_msg() const override
        {
                return "I am no longer terrified of reptiles and amphibians.";
        }

        std::string history_msg() const override
        {
                return "Gained a phobia of reptiles and amphibians";
        }

        std::string history_msg_end() const override
        {
                return "My phobia of reptiles and amphibians was cured";
        }
};

class InsPhobiaCanine : public InsSympt
{
public:
        InsPhobiaCanine() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_canine;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "Phobia of canines";
        }

        std::string postmortem_msg() const override
        {
                return "Had a phobia of canines";
        }

protected:
        std::string start_msg() const override
        {
                return "Canines suddenly seem terrifying.";
        }

        std::string start_heading() const override
        {
                return "Cynophobia!";
        }

        std::string end_msg() const override
        {
                return "I am no longer terrified of canines.";
        }

        std::string history_msg() const override
        {
                return "Gained a phobia of canines";
        }

        std::string history_msg_end() const override
        {
                return "My phobia of canines was cured";
        }
};

class InsPhobiaDead : public InsSympt
{
public:
        InsPhobiaDead() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_dead;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "Phobia of the dead";
        }

        std::string postmortem_msg() const override
        {
                return "Had a phobia of the dead";
        }

protected:
        std::string start_msg() const override
        {
                return "The dead suddenly seem far more terrifying.";
        }

        std::string start_heading() const override
        {
                return "Necrophobia!";
        }

        std::string end_msg() const override
        {
                return "I am no longer terrified of the dead.";
        }

        std::string history_msg() const override
        {
                return "Gained a phobia of the dead";
        }

        std::string history_msg_end() const override
        {
                return "My phobia of the dead was cured";
        }
};

class InsPhobiaDeep : public InsSympt
{
public:
        InsPhobiaDeep() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_deep;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "Phobia of deep places";
        }

        std::string postmortem_msg() const override
        {
                return "Had a phobia of deep places";
        }

protected:
        std::string start_msg() const override
        {
                return "It suddenly seems far more terrifying to delve deeper.";
        }

        std::string start_heading() const override
        {
                return "Bathophobia!";
        }

        std::string end_msg() const override
        {
                return "I am no longer terrified of deep places.";
        }

        std::string history_msg() const override
        {
                return "Gained a phobia of deep places";
        }

        std::string history_msg_end() const override
        {
                return "My phobia of deep places was cured";
        }
};

class InsPhobiaDark : public InsSympt
{
public:
        InsPhobiaDark() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_dark;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_foes) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "Phobia of darkness";
        }

        std::string postmortem_msg() const override
        {
                return "Had a phobia of darkness";
        }

protected:
        std::string start_msg() const override
        {
                return "Darkness suddenly seems far more terrifying.";
        }

        std::string start_heading() const override
        {
                return "Nyctophobia!";
        }

        std::string end_msg() const override
        {
                return "I am no longer terrified of darkness.";
        }

        std::string history_msg() const override
        {
                return "Gained a phobia of darkness";
        }

        std::string history_msg_end() const override
        {
                return "My phobia of darkness was cured";
        }
};

class InsSadism : public InsSympt
{
public:
        InsSadism() = default;

        InsSymptId id() const override
        {
                return InsSymptId::sadism;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return true;
        }

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "Sadistic obsession";
        }

        std::string postmortem_msg() const override
        {
                return "Had a sadistic obsession";
        }

protected:
        std::string start_msg() const override
        {
                return "To my alarm, I find myself encouraged by the pain I "
                       "cause in others. For every significant life I take, I "
                       "find a little relief. However, my depraved mind will "
                       "never find complete peace.";
        }

        std::string start_heading() const override
        {
                return "Sadistic obsession!";
        }

        std::string end_msg() const override
        {
                return "I am cured of my sadistic obsession.";
        }

        std::string history_msg() const override
        {
                return "Gained a sadistic obsession";
        }

        std::string history_msg_end() const override
        {
                return "My sadistic obsession was cured";
        }
};

class InsShadows : public InsSympt
{
public:
        InsShadows() = default;

        InsSymptId id() const override
        {
                return InsSymptId::shadows;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "The shadows are closing in on me!";
        }

        std::string start_heading() const override
        {
                return "Haunted by shadows!";
        }

        std::string history_msg() const override
        {
                return "Was haunted by shadows";
        }
};

class InsParanoia : public InsSympt
{
public:
        InsParanoia() = default;

        InsSymptId id() const override
        {
                return InsSymptId::paranoia;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "Is there someone following me? Or is it panic taking "
                       "over?";
        }

        std::string start_heading() const override
        {
                return "Paranoia!";
        }

        std::string history_msg() const override
        {
                return "Had a strong sensation of being followed";
        }
};

class InsConfusion : public InsSympt
{
public:
        InsConfusion() = default;

        InsSymptId id() const override
        {
                return InsSymptId::confusion;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "I find myself in a peculiar trance. I struggle to "
                       "recall where I am, and what is happening.";
        }

        std::string start_heading() const override
        {
                return "Confusion!";
        }

        std::string history_msg() const override
        {
                return "Suddenly felt deeply confused for no reason";
        }
};

class InsFrenzy : public InsSympt
{
public:
        InsFrenzy() = default;

        InsSymptId id() const override
        {
                return InsSymptId::frenzy;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "I fall into an uncontrollable rage!";
        }

        std::string start_heading() const override
        {
                return "Frenzy!";
        }

        std::string history_msg() const override
        {
                return "Fell into an uncontrollable rage";
        }
};

class InsStrangeSensation : public InsSympt
{
public:
        InsStrangeSensation() = default;

        InsSymptId id() const override
        {
                return InsSymptId::strange_sensation;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

protected:
        std::string start_msg() const override
        {
                return "There is a strange itch, as if something is crawling "
                       "on the back of my neck.";
        }

        std::string start_heading() const override
        {
                return "Strange sensation!";
        }

        std::string history_msg() const override
        {
                return "Had a sensation of something crawling on my neck";
        }
};

namespace insanity
{
void init();
void cleanup();

void save();
void load();

void run_sympt();

bool has_sympt(InsSymptId id);

bool has_sympt_type(InsSymptType type);

std::vector<const InsSympt*> active_sympts();

void on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes);

void on_permanent_rfear();

void end_sympt(InsSymptId id);

}  // namespace insanity

#endif  // INSANITY_HPP
