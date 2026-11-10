#pragma once
#include "../../../third_party/R2JUCE/modules/R2JUCE/src/R2SequencePlayer.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>

// R2JUCE owns transport and event timing. This adapter owns file lifetimes:
// parse/destroy on the loader thread, and acknowledge replacement on audio.
class SequencePlayer
{
public:
    bool load(const std::string& path, std::string& error)
    {
        auto next=std::make_unique<r2juce::SequenceFile>();
        if (!next->load(path,error,false)) return false;
        std::lock_guard<std::mutex> lock(loadMutex);
        int slot=-1;
        for(int i=0;i<int(slots.size());++i)
            if(slots[size_t(i)].state.load(std::memory_order_acquire)==freeSlot) {slot=i;break;}
        if(slot<0) {error="Player replacement busy; try again";return false;}
        slots[size_t(slot)].data=std::move(next);
        slots[size_t(slot)].state.store(ownedSlot,std::memory_order_release);
        command.store(stopCommand,std::memory_order_release);
        const int old=pending.exchange(slot,std::memory_order_acq_rel);
        if(old>=0) slots[size_t(old)].state.store(freeSlot,std::memory_order_release);
        loaded.store(true,std::memory_order_release);
        return true;
    }
    void play() {if(hasFile()) command.store(playCommand,std::memory_order_release);}
    void pause() {command.store(pauseCommand,std::memory_order_release);}
    void stop() {command.store(stopCommand,std::memory_order_release);}
    bool hasFile() const {return loaded.load(std::memory_order_acquire);}
    bool isPlaying() const {
        const int c=command.load(std::memory_order_acquire);
        return c==none ? player.isPlaying() : c==playCommand;
    }
    double seconds() const {return command.load()==stopCommand?0:player.getPositionSeconds();}
    bool takeFinished() {return player.takeFinished();}

    // Audio thread only. send(data,size,sampleOffset,port); other() runs after reset
    // controllers and before the song's events, as specified by R2JUCE.
    template<class SendAt, class Other>
    void processStamped(int samples,double rate,SendAt&& send,Other&& other)
    {
        int retired=-1;
        const int next=pending.exchange(-1,std::memory_order_acq_rel);
        if(next>=0) {
            retired=activeSlot; activeSlot=next;
            player.load(slots[size_t(next)].data.get());
        }
        switch(command.exchange(none,std::memory_order_acq_rel)) {
            case playCommand: player.play();break;
            case pauseCommand: player.pause();break;
            case stopCommand: player.stop();break;
            default:break;
        }
        player.processStamped(samples,rate,send,other);
        // R2JUCE has consumed load() now and no longer references the old file.
        if(retired>=0) slots[size_t(retired)].state.store(freeSlot,std::memory_order_release);
    }
private:
    enum {none,playCommand,pauseCommand,stopCommand};
    enum {freeSlot,ownedSlot};
    struct Slot {std::unique_ptr<r2juce::SequenceFile> data;std::atomic<int> state{freeSlot};};
    std::array<Slot,3> slots;
    std::mutex loadMutex;
    std::atomic<int> pending{-1},command{none};
    std::atomic<bool> loaded{false};
    int activeSlot=-1;
    r2juce::SequencePlayer player;
};
