#pragma once

// ======================================================
// FISHIX KERNEL ROOT HEADER
// included by ALL kernel subsystems
// ======================================================

// ---- basic klib ----
#include <klib/common.hpp>
#include <klib/cstring.hpp>
#include <klib/vector.hpp>
#include <klib/list.hpp>
#include <klib/lock.hpp>

// ---- core kernel ----
#include <panic.hpp>
#include <limine.hpp>

// ---- memory ----
#include <mem/pmm.hpp>
#include <mem/vmm.hpp>
#include <mem/dma.hpp>

// ---- scheduler ----
#include <sched/sched.hpp>
#include <sched/context.hpp>

// ---- filesystem ----
#include <fs/vfs.hpp>