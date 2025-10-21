// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"

@implementation ImageContentView {
  ImageContentConfiguration* _configuration;
  // The width constraint of the image view.
  NSLayoutConstraint* _widthConstraint;

  // The height constraint of the image view.
  NSLayoutConstraint* _heightConstraint;
}

- (instancetype)initWithConfiguration:
    (ImageContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _widthConstraint = [self.widthAnchor
        constraintEqualToConstant:configuration.imageSize.width];
    _heightConstraint = [self.heightAnchor
        constraintEqualToConstant:configuration.imageSize.height];

    [NSLayoutConstraint activateConstraints:@[
      _widthConstraint,
      _heightConstraint,
    ]];

    _configuration = configuration;

    [self applyConfiguration];
  }
  return self;
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return NO;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  _configuration =
      base::apple::ObjCCastStrict<ImageContentConfiguration>(configuration);
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration isMemberOfClass:ImageContentConfiguration.class];
}

#pragma mark - Private

- (void)applyConfiguration {
  self.image = _configuration.image;
  self.contentMode = _configuration.imageContentMode;
  _widthConstraint.constant = _configuration.imageSize.width;
  _heightConstraint.constant = _configuration.imageSize.height;
}

@end
