//===--- AvailabilityContext.cpp - Swift Availability Structures ----------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/AST/AvailabilityContext.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/AvailabilityContextStorage.h"
#include "swift/AST/Decl.h"
#include "swift/Basic/Assertions.h"

using namespace swift;

bool AvailabilityContext::PlatformInfo::constrainRange(const Decl *decl) {
  if (auto range = AvailabilityInference::annotatedAvailableRange(decl))
    return constrainRange(*range);

  return false;
}

bool AvailabilityContext::PlatformInfo::constrainUnavailability(
    const Decl *decl) {
  auto &ctx = decl->getASTContext();
  auto *attr = decl->getAttrs().getUnavailable(ctx);
  if (!attr)
    return false;

  // Check whether the decl's unavailability reason is the same.
  if (IsUnavailable && UnavailablePlatform == attr->Platform)
    return false;

  // Check whether the decl's unavailability reason is more specific.
  if (attr->Platform != PlatformKind::none &&
      inheritsAvailabilityFromPlatform(attr->Platform, UnavailablePlatform))
    return false;

  IsUnavailable = true;
  UnavailablePlatform = attr->Platform;
  return true;
}

bool AvailabilityContext::PlatformInfo::constrainDeprecated(const Decl *decl) {
  auto &ctx = decl->getASTContext();
  if (IsDeprecated || !decl->getAttrs().isDeprecated(ctx))
    return false;

  IsDeprecated = true;
  return true;
}

bool AvailabilityContext::PlatformInfo::isContainedIn(
    const PlatformInfo &other) const {
  if (!Range.isContainedIn(other.Range))
    return false;

  if (!IsUnavailable && other.IsUnavailable)
    return false;

  if (IsUnavailable && other.IsUnavailable) {
    if (UnavailablePlatform != other.UnavailablePlatform &&
        UnavailablePlatform != PlatformKind::none &&
        !inheritsAvailabilityFromPlatform(UnavailablePlatform,
                                          other.UnavailablePlatform))
      return false;
  }

  if (!IsDeprecated && other.IsDeprecated)
    return false;

  return true;
}

void AvailabilityContext::Storage::Profile(llvm::FoldingSetNodeID &id) const {
  Platform.Profile(id);
}

AvailabilityContext AvailabilityContext::getDefault(ASTContext &ctx) {
  PlatformInfo platformInfo{AvailabilityRange::forInliningTarget(ctx),
                            PlatformKind::none,
                            /*IsUnavailable*/ false,
                            /*IsDeprecated*/ false};
  return AvailabilityContext(Storage::get(platformInfo, ctx));
}

AvailabilityContext
AvailabilityContext::get(const AvailabilityRange &platformAvailability,
                         std::optional<PlatformKind> unavailablePlatform,
                         bool deprecated, ASTContext &ctx) {
  PlatformInfo platformInfo{platformAvailability,
                            unavailablePlatform.has_value()
                                ? *unavailablePlatform
                                : PlatformKind::none,
                            unavailablePlatform.has_value(), deprecated};
  return AvailabilityContext(Storage::get(platformInfo, ctx));
}

AvailabilityRange AvailabilityContext::getPlatformRange() const {
  return Info->Platform.Range;
}

std::optional<PlatformKind>
AvailabilityContext::getUnavailablePlatformKind() const {
  if (Info->Platform.IsUnavailable)
    return Info->Platform.UnavailablePlatform;
  return std::nullopt;
}

bool AvailabilityContext::isDeprecated() const {
  return Info->Platform.IsDeprecated;
}

void AvailabilityContext::constrainWithPlatformRange(
    const AvailabilityRange &platformRange, ASTContext &ctx) {
  PlatformInfo platformAvailability{Info->Platform};
  if (!platformAvailability.constrainRange(platformRange))
    return;

  Info = Storage::get(platformAvailability, ctx);
}

void AvailabilityContext::constrainWithDeclAndPlatformRange(
    Decl *decl, const AvailabilityRange &platformRange) {
  PlatformInfo platformAvailability{Info->Platform};
  bool isConstrained = false;
  isConstrained |= platformAvailability.constrainRange(decl);
  isConstrained |= platformAvailability.constrainRange(platformRange);
  isConstrained |= platformAvailability.constrainUnavailability(decl);
  isConstrained |= platformAvailability.constrainDeprecated(decl);

  if (!isConstrained)
    return;

  Info = Storage::get(platformAvailability, decl->getASTContext());
}

bool AvailabilityContext::isContainedIn(const AvailabilityContext other) const {
  if (!Info->Platform.isContainedIn(other.Info->Platform))
    return false;

  return true;
}

static std::string
stringForAvailability(const AvailabilityRange &availability) {
  if (availability.isAlwaysAvailable())
    return "all";
  if (availability.isKnownUnreachable())
    return "none";

  return availability.getVersionString();
}

void AvailabilityContext::print(llvm::raw_ostream &os) const {
  os << "version=" << stringForAvailability(getPlatformRange());

  if (auto unavailablePlatform = getUnavailablePlatformKind())
    os << " unavailable=" << platformString(*unavailablePlatform);

  if (isDeprecated())
    os << " deprecated";
}

void AvailabilityContext::dump() const { print(llvm::errs()); }
