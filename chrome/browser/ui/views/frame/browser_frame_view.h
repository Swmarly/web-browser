// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_H_

#include <ostream>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/window/frame_view.h"

class BrowserView;

namespace views {
class Label;
}

// This enum is used for functions who rely on the state of the browser to alter
// the appearance of the window frame.
enum class BrowserFrameActiveState {
  // Use the window's actual current active/inactive state.
  kUseCurrent,
  // Force the frame to be treated as active, regardless of the current state.
  // Note: Only used on ChromeOS.
  kActive,
  // Force the frame to be treated as inactive, regardless of the current sate.
  // Note: Only used on ChromeOS.
  kInactive,
};

// Represents an area in the upper left or right of the browser window that
// browser UI should be careful when rendering in. This might include caption
// buttons, control box, or app icon.
//
// This is an example of the leading exclusion area in LTR:
//
// ┏━━━━━━━━━━━━━━━━┯━━━━━━━━━━━━┯━━━━━
// ┃ content        │ horizontal │
// ┠────────────────┘     ↔      │
// ┃    vertical ↕     padding   ┊
// ┠─────────────────┄┄┄┄┄┄┄┄┄┄┄┄┘
// ┃
//
struct BrowserLayoutExclusionArea {
  // This is the area which has visual elements managed by the frame. No drawing
  // should occur here.
  gfx::SizeF content;

  // Any additional area next to the content that should remain empty for visual
  // balance. It's okay for edges and borders to be drawn in this space.
  float horizontal_padding = 0.f;

  // Any additional area below the content that should remain empty for visual
  // balance. It's okay for edges and borders to be drawn in this space.
  float vertical_padding = 0.f;

  // Returns the content area plus the padding, if any.
  gfx::SizeF ContentWithPadding() const {
    return gfx::SizeF(content.width() + horizontal_padding,
                      content.height() + vertical_padding);
  }

  // Returns true if there is no exclusion area.
  bool IsEmpty() const { return ContentWithPadding().IsEmpty(); }
};

// Represents the parameters that the browser's layout requires in order to lay
// out the window contents.
//
// This is how the exclusion areas look in LTR:
// ┏━━━━━━━━━━━━━━━━━━━┯━━━━━━━━━━━━━━┯━━━━━━━━━━━━━━━━━━━━┓
// ┃ leading_exclusion │              │ trailing_exclusion ┃
// ┠───────────────────┘              └────────────────────┨
// ┃                                                       ┃
//
// Note that in RTL UI, coordinates are reversed, so the leading exclusion is
// still at the lower X coordinate and the trailing exclusion at the higher.
//
// Also note that one or both exclusions may be empty, in which case there is
// no exclusion.
//
struct BrowserLayoutParams {
  // A rectangle in which it is generally safe to lay out browser view elements.
  // This is in window coordinates and may not align with the actual content
  // view. It is okay for the content view to paint outside this rectangle, but
  // that may overlap OS or frame elements.
  gfx::Rect visual_client_area;
  // The area in the leading (lowest X values; i.e. top-left in LTR and top-
  // right in RTL) corner occupied by frame-owned controls, from the edge of the
  // visual client area.
  //
  // It is sometimes okay for the content to draw through the edge of this area,
  // e.g. to draw the leading curve of the first tab. Use the difference between
  // `content` and `content_with_padding` to determine the area it is safe to
  // draw in.
  BrowserLayoutExclusionArea leading_exclusion;
  // The area in the trailing (highest X values; i.e. top-right in LTR and top-
  // left in RTL) corner occupied by frame-owned controls, from the edge of the
  // visual client area.
  //
  // It is sometimes okay for the content to draw through the edge of this area,
  // e.g. to draw the leading curve of the first tab. Use the difference between
  // `content` and `content_with_padding` to determine the area it is safe to
  // draw in.
  BrowserLayoutExclusionArea trailing_exclusion;
};

// BrowserFrameView is an abstract base class that defines the
// interface for the part of a browser window that is not the "client area"
// (where the web content is displayed). This includes the title bar, window
// borders, and caption buttons (minimize, maximize, close).
//
// This class is responsible for:
// - Laying out major UI components like the tab strip.
// - Painting the window frame, taking into account the browser theme.
// - Responding to window state changes (fullscreen, activation, maximization).
//
// Concrete implementations are provided for each platform (e.g., Windows, Mac,
// Linux) and are created by the factory function
// `chrome::CreateBrowserFrameView`.
class BrowserFrameView : public views::FrameView {
  METADATA_HEADER(BrowserFrameView, views::FrameView)

 public:
  BrowserFrameView(BrowserWidget* browser_widget, BrowserView* browser_view);
  BrowserFrameView(const BrowserFrameView&) = delete;
  BrowserFrameView& operator=(const BrowserFrameView&) = delete;
  ~BrowserFrameView() override;

  BrowserView* browser_view() const { return browser_view_; }
  BrowserWidget* browser_widget() const { return browser_widget_; }

  // Called after BrowserView has initialized its child views. This is a useful
  // hook for performing final setup that depends on other child views, like
  // the tabstrip or toolbar, being present.
  virtual void OnBrowserViewInitViewsComplete();

  // Called when the browser window's fullscreen state changes.
  virtual void OnFullscreenStateChanged();

  // Returns whether the caption buttons are drawn at the leading edge (e.g. on
  // the left for LTR languages, such as on macOS).
  virtual bool CaptionButtonsOnLeadingEdge() const;

  // Default implementation for getting browser layout parameters.
  virtual BrowserLayoutParams GetBrowserLayoutParams() const;

  // Returns the bounds, in this view's coordinates, that the tab
  // strip should occupy.
  virtual gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const = 0;

  // Returns the maximum bounds, in this view's coordinates, for
  // the WebAppFrameToolbarView, which contains controls for a web app.
  virtual gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const = 0;

  // Lays out the window title for a web app within the given available space.
  // Unlike the above GetBounds methods this is not just a method to return the
  // bounds the title should occupy, since different implementations might also
  // want to change other attributes of the title, such as alignment.
  virtual void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                                       views::Label& window_title_label) const;

  // Returns the inset from the top of the window to the top of the client
  // view. For a tabbed browser, this is the space occupied by the tab strip.
  // For popup windows, this is the toolbar. For app windows, this is the
  // WebContents. Varies on fullscreen. If |restored| is true, this is
  // calculated for the window's restored state, regardless of its current state
  // (e.g., maximized or fullscreen).
  virtual int GetTopInset(bool restored) const = 0;

  // Updates the top UI state to be hidden or shown in fullscreen according to
  // the preference's state. Currently only used on Mac.
  virtual void UpdateFullscreenTopUI();

  // Returns true if the top UI (tabstrip, toolbar) should be hidden because the
  // browser is in fullscreen mode.
  virtual bool ShouldHideTopUIInFullscreen() const;

  // Returns true if a toolbar should be shown in the current browser, false if
  // not. If this returns false, there is no reason to call e.g.
  // `GetBoundsForWebAppFrameToolbar()`.
  virtual bool ShouldShowWebAppFrameToolbar() const;

  // Determines whether the top of the frame is "condensed" (i.e., has less
  // vertical space). This is typically true when the window is maximized or
  // fullscreen. If true, the top frame is just the height of a tab,
  // rather than having extra vertical space above the tabs.
  virtual bool IsFrameCondensed() const;

  // Determines if background tab shapes have a distinct appearance from the
  // frame background. This is true if the theme uses a custom tab background
  // image or if the calculated color for background tabs differs from the frame
  // color.
  bool HasVisibleBackgroundTabShapes(
      BrowserFrameActiveState active_state) const;

  // Returns the color that should be used for text and icons in the title bar
  // (e.g., the window title and caption button icons).
  virtual SkColor GetCaptionColor(BrowserFrameActiveState active_state) const;

  // Returns the primary background color of the browser frame. This is also the
  // color used for the tab strip background unless overridden by a theme.
  virtual SkColor GetFrameColor(BrowserFrameActiveState active_state) const;

  // Returns the resource ID for a custom background image if the active theme
  // provides one for the frame. This checks for images for the given active
  // state and also considers theme-related fallbacks (e.g., an inactive image
  // falling back to an active one).
  std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const;

  // Updates the the loading animation (throbber) for the window icon in the
  // frame. Mainly used by special browsers such as PWAs.
  virtual void UpdateThrobber(bool running) = 0;

  // Signals that the frame's minimum size may have changed. This prompts the
  // widget to update its size constraints by re-querying `GetMinimumSize()`.
  // This is typically called when child views (e.g. tab strip, toolbar,
  // bookmarks bar) change visibility.The window manager is notified of this
  // change via GetWidget()->OnSizeConstraintsChanged().
  virtual void UpdateMinimumSize();

  // Called when the Window Controls Overlay state changes, allowing the frame
  // to update the state of the caption buttons accordingly.
  virtual void WindowControlsOverlayEnabledChanged() {}

  // Returns the insets from the edge of the native window to the client view in
  // DIPs. The value is left-to-right even on RTL locales. That is,
  // insets.left() will be on the left in screen coordinates. Subclasses must
  // implement this.
  virtual gfx::Insets RestoredMirroredFrameBorderInsets() const;

  // Returns the insets from the client view to the input region. The returned
  // insets will be negative, such that view_rect.Inset(GetInputInsets()) will
  // be the input region. Subclasses must implement this.
  virtual gfx::Insets GetInputInsets() const;

  // Gets the rounded-rect clipping region for the window frame when it is
  // in its restored (non-maximized) state. Subclasses must implement this.
  virtual SkRRect GetRestoredClipRegion() const;

  // Returns the height of the translucent area at the top of the frame. Returns
  // 0 if the frame is opaque (not transparent) or in fullscreen.
  virtual int GetTranslucentTopAreaHeight() const;

  // Sets the bounds of `frame_`.
  virtual void SetFrameBounds(const gfx::Rect& bounds);

  // views::FrameView:
  void Layout(PassKey) override;
  Views GetChildrenInZOrder() override;

 protected:
  // Called when `frame_`'s "paint as active" state has changed.
  virtual void PaintAsActiveChanged();

  // Used by GetCaptionButtonBounds() below.
  struct BoundsAndMargins {
    // The bounds of a view or collection of views.
    gfx::RectF bounds;
    // The preferred margins around `bounds`.
    gfx::OutsetsF margins;

    // Returns the rectangle that contains `bounds` with `margins`.
    gfx::Rect ToEnclosingRect() const;
  };

  // Gets the bounds of the caption buttons, and their required margins if any.
  // The bounds are the combined rectangle containing all caption buttons; the
  // margins are the preferred visual padding area around that rectangle.
  //
  // Mac (small buttons; padding around):
  //
  // ┏━━━━━━━━━━━━━┯━━━━━━
  // ┃  ┌───────┐  │
  // ┃  │O  O  O│  │
  // ┃  └───────┘  │
  // ┠─────────────┘
  //
  // Windows (larger buttons, no additional padding):
  //
  // ━━━━━┯━━━━━━━━━━━━━┓
  //      │  _   □   X  ┃
  //      └─────────────┨
  //                    ┃
  //
  virtual BoundsAndMargins GetCaptionButtonBounds() const;

  // Helper function to determine if we should treat the frame as the active
  // state.
  bool ShouldPaintAsActiveForState(BrowserFrameActiveState active_state) const;

  // Returns a themed image for the frame background, if one exists.
  gfx::ImageSkia GetFrameImage(BrowserFrameActiveState active_state =
                                   BrowserFrameActiveState::kUseCurrent) const;

  // Returns a themed image for the frame overlay, if one exists.
  gfx::ImageSkia GetFrameOverlayImage(
      BrowserFrameActiveState active_state =
          BrowserFrameActiveState::kUseCurrent) const;

 private:
#if BUILDFLAG(IS_WIN)
  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::FrameView:
  int GetSystemMenuY() const override;
#endif  // BUILDFLAG(IS_WIN)

  // The BrowserWidget that owns this view.
  const raw_ptr<BrowserWidget, DanglingUntriaged> browser_widget_;

  // The BrowserView hosted within `frame_`.
  const raw_ptr<BrowserView, DanglingUntriaged> browser_view_;

  // Subscription to receive notifications when the frame's PaintAsActive state
  // changes.
  base::CallbackListSubscription paint_as_active_subscription_ =
      browser_widget_->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(&BrowserFrameView::PaintAsActiveChanged,
                              base::Unretained(this)));
};

namespace chrome {

// Factory function for creating a BrowserFrameView. Platform specific
// implementations should define this in their respective
// browser_view_factor_*.cc files.
std::unique_ptr<BrowserFrameView> CreateBrowserFrameView(
    BrowserWidget* browser_widget,
    BrowserView* browser_view);

}  // namespace chrome

// For debugging and testing.
std::ostream& operator<<(std::ostream&, const BrowserLayoutExclusionArea&);
std::ostream& operator<<(std::ostream&, const BrowserLayoutParams&);

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_H_
