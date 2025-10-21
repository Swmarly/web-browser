// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet_detents_interactor.h"

#import "base/check.h"

@implementation LensOverlayBottomSheetDetentInteractor {
  // The presentation controller that manages the appearance and behavior of
  // the gottom sheet.
  __weak UISheetPresentationController* _sheetPresentationController;
  __weak id<LensOverlayBottomSheet> _lensOverlayBottomSheet;
}

- (instancetype)initWithSystemSheetPresentationController:
    (UISheetPresentationController*)sheetPresentationController {
  self = [super init];
  if (self) {
    _sheetPresentationController = sheetPresentationController;
    _usesSystemPresentation = YES;
  }

  return self;
}

- (instancetype)initWithLensOverlayBottomSheet:
    (id<LensOverlayBottomSheet>)bottomSheet {
  self = [super init];
  if (self) {
    _lensOverlayBottomSheet = bottomSheet;
    _usesSystemPresentation = NO;
  }

  return self;
}

#pragma mark - Public

- (UISheetPresentationControllerDetentIdentifier)selectedDetentIdentifier {
  if (_lensOverlayBottomSheet) {
    return _lensOverlayBottomSheet.selectedDetentIdentifier;
  } else {
    return _sheetPresentationController.selectedDetentIdentifier;
  }
}

- (void)setSelectedDetentIdentifier:
            (UISheetPresentationControllerDetentIdentifier)
                selectedDetentIdentifier
                           animated:(BOOL)animated {
  if (_lensOverlayBottomSheet) {
    [_lensOverlayBottomSheet
        setSelectedDetentIdentifier:selectedDetentIdentifier
                           animated:animated];
  } else {
    if (animated) {
      __weak __typeof(_sheetPresentationController) weakPresentationController =
          _sheetPresentationController;
      [_sheetPresentationController animateChanges:^{
        weakPresentationController.selectedDetentIdentifier =
            selectedDetentIdentifier;
      }];
    } else {
      _sheetPresentationController.selectedDetentIdentifier =
          selectedDetentIdentifier;
    }
  }
}

- (void)setDetents:(NSArray<LensOverlayBottomSheetDetentProxy*>*)detents {
  if (_lensOverlayBottomSheet) {
    NSMutableArray<LensOverlayBottomSheetDetent*>* lensOverlayDetents =
        [NSMutableArray array];
    for (LensOverlayBottomSheetDetentProxy* proxyDetent in detents) {
      [lensOverlayDetents addObject:proxyDetent.lensOverlayDetent];
    }
    _lensOverlayBottomSheet.detents = lensOverlayDetents;
  } else {
    NSMutableArray<UISheetPresentationControllerDetent*>* systemDetents =
        [NSMutableArray array];
    for (LensOverlayBottomSheetDetentProxy* proxyDetent in detents) {
      [systemDetents addObject:proxyDetent.systemDetent];
    }
    _sheetPresentationController.detents = systemDetents;
  }
}

- (void)setLargestUndimmedDetentIdentifier:
    (UISheetPresentationControllerDetentIdentifier)
        largestUndimmedDetentIdentifier {
  _sheetPresentationController.largestUndimmedDetentIdentifier =
      largestUndimmedDetentIdentifier;
}

- (void)animateChanges:(ProceduralBlock)changes {
  if (_sheetPresentationController) {
    [_sheetPresentationController animateChanges:changes];
  } else {
    changes();
  }
}

- (LensOverlayBottomSheetDetentProxy*)
    detentWithIdentifier:
        (UISheetPresentationControllerDetentIdentifier)identifier
                  height:(CGFloat)height {
  return [self detentWithIdentifier:identifier
                     heightResolver:^{
                       return height;
                     }];
}

- (LensOverlayBottomSheetDetentProxy*)
    detentWithIdentifier:
        (UISheetPresentationControllerDetentIdentifier)identifier
          heightResolver:(CGFloat (^)())heightResolver {
  CHECK(heightResolver);
  if (_sheetPresentationController) {
    auto infoMessageHeightResolver = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      return heightResolver();
    };

    UISheetPresentationControllerDetent* detent =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:identifier
                              resolver:infoMessageHeightResolver];
    return
        [[LensOverlayBottomSheetDetentProxy alloc] initWithSystemDetent:detent];
  } else {
    LensOverlayBottomSheetDetent* detent = [[LensOverlayBottomSheetDetent alloc]
        initWithIdentifier:identifier
             valueResolver:heightResolver];
    return [[LensOverlayBottomSheetDetentProxy alloc]
        initWithLensOverlayDetent:detent];
  }
}

@end
