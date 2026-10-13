#include "OvertimeChecks.hpp"
#include "FlamethrowerChecks.hpp"
#include "WeaponCardProgressionChecks.hpp"
#include "BossBattleChecks.hpp"
#include "RunUpgradeRetirementChecks.hpp"

int main() {
    checkOvertime();
    checkFlamethrowerBuilding();
    checkWeaponCardProgression();
    checkRetiredHighStakes();
    checkAlpakingBattle();
}
