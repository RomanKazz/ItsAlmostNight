#include "OvertimeChecks.hpp"
#include "WeaponCardProgressionChecks.hpp"
#include "BossBattleChecks.hpp"
#include "RunUpgradeRetirementChecks.hpp"

int main() {
    checkOvertime();
    checkWeaponCardProgression();
    checkRetiredHighStakes();
    checkAlpakingBattle();
}
