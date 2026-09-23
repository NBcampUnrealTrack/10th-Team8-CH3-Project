// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/EngineTypes.h"

namespace MonsterCollision
{
    // 반드시 DefaultEngine.ini의 실제 채널 번호에 맞춰줘.
    constexpr ECollisionChannel Monster = ECC_GameTraceChannel1;
    constexpr ECollisionChannel MonsterProjectile = ECC_GameTraceChannel2;
    constexpr ECollisionChannel WeaponTrace = ECC_GameTraceChannel3;
    constexpr ECollisionChannel MonsterHitbox = ECC_GameTraceChannel4;
}