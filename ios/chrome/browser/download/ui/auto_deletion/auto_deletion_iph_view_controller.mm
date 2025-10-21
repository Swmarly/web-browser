// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_iph_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_mutator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/auto_deletion_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// The size of the trash icon.
constexpr CGFloat kTrashSymbolSize = 32.0;
// The border radius of the icon's container.
constexpr CGFloat kSymbolBackgroundCornerRadius = 15.0;
// The height and width of the trash symbol's container.
constexpr CGFloat kSymbolContainerSize = 64;
// The border radius of the half-sheet.
constexpr CGFloat kPreferredCornerRadius = 20;
// The spacing between the top of the half-sheet and the dismiss button.
constexpr CGFloat kDismissButtonTopPaddng = 8;

// Creates a UIView that contains the the IPH's icon as a subview.
UIView* CreateIconContainer() {
  // Create the trash icon.
  UIImage* symbol =
      DefaultSymbolWithPointSize(kArrowUpTrashSymbol, kTrashSymbolSize);
  UIImageView* symbolView = [[UIImageView alloc] initWithImage:symbol];
  symbolView.translatesAutoresizingMaskIntoConstraints = NO;
  [symbolView setTintColor:UIColor.whiteColor];

  // Create the retaining UIView that decorates the icon.
  UIView* symbolBackground = [[UIView alloc] init];
  symbolBackground.backgroundColor = [UIColor colorNamed:kPurple500Color];
  symbolBackground.layer.cornerRadius = kSymbolBackgroundCornerRadius;
  symbolBackground.translatesAutoresizingMaskIntoConstraints = NO;
  [symbolBackground addSubview:symbolView];

  // Center the icon within the container.
  [NSLayoutConstraint activateConstraints:@[
    [symbolBackground.widthAnchor
        constraintEqualToConstant:kSymbolContainerSize],
    [symbolBackground.heightAnchor
        constraintEqualToConstant:kSymbolContainerSize],
  ]];
  AddSameCenterConstraints(symbolBackground, symbolView);

  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:symbolBackground];
  // Center the icon within the container.
  [NSLayoutConstraint activateConstraints:@[
    [container.widthAnchor
        constraintGreaterThanOrEqualToAnchor:symbolBackground.widthAnchor],
    [container.heightAnchor
        constraintGreaterThanOrEqualToAnchor:symbolBackground.heightAnchor],
  ]];
  AddSameCenterConstraints(container, symbolBackground);

  return container;
}

}  // namespace

@interface AutoDeletionIPHViewController () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate> {
  // A pointer to the browser object.
  raw_ptr<Browser> _browser;
}

@end

@implementation AutoDeletionIPHViewController {
  ConfirmationAlertViewController* _iphScreen;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _browser = browser;
  }

  return self;
}

- (void)viewDidLoad {
  _iphScreen = [[ConfirmationAlertViewController alloc] init];
  _iphScreen.titleString =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_TITLE);
  _iphScreen.subtitleString =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_DESCRIPTION);
  _iphScreen.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_PRIMARY_ACTION);
  _iphScreen.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_AUTO_DELETION_IPH_REJECTION);
  _iphScreen.aboveTitleView = CreateIconContainer();
  _iphScreen.dismissBarButtonSystemItem = UIBarButtonSystemItemClose;
  _iphScreen.actionHandler = self;
  _iphScreen.presentationController.delegate = self;

  [self addChildViewController:_iphScreen];
  [self.view addSubview:_iphScreen.view];
  [_iphScreen didMoveToParentViewController:self];
  [super viewDidLoad];
  [self layoutAlertScreen];
}

#pragma mark - ConfirmationAlertActionHandler

// Enables the Auto-deletion feature and displays the Auto-deletion
// action-sheet.
- (void)confirmationAlertPrimaryAction {
  base::RecordAction(
      base::UserMetricsAction("IOS.AutoDeletion.IPH.EnableButtonTapped"));
  [self.mutator enableAutoDeletion];
}

// Dismisses the IPH.
- (void)confirmationAlertDismissAction {
  base::RecordAction(
      base::UserMetricsAction("IOS.AutoDeletion.IPH.DismissButtonTapped"));
  id<AutoDeletionCommands> handler = HandlerForProtocol(
      _browser->GetCommandDispatcher(), AutoDeletionCommands);
  [handler dismissAutoDeletionActionSheet];
}

// Does not enable the Auto-deletion feature and dismisses the IPH.
- (void)confirmationAlertSecondaryAction {
  base::RecordAction(
      base::UserMetricsAction("IOS.AutoDeletion.IPH.RejectButtonTapped"));
  id<AutoDeletionCommands> handler = HandlerForProtocol(
      _browser->GetCommandDispatcher(), AutoDeletionCommands);
  [handler dismissAutoDeletionActionSheet];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  id<AutoDeletionCommands> handler = HandlerForProtocol(
      _browser->GetCommandDispatcher(), AutoDeletionCommands);
  [handler dismissAutoDeletionActionSheet];
}

#pragma mark - Private

// Sets the layout of the alertScreen view when the promo will be
// shown without the animation view (half-screen promo).
- (void)layoutAlertScreen {
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  _iphScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_iphScreen.view.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                              constant:kDismissButtonTopPaddng],
    [_iphScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_iphScreen.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_iphScreen.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
  presentationController.preferredCornerRadius = kPreferredCornerRadius;
}

@end
