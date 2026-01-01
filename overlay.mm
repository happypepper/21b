#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <cmath>

@interface OverlayLabel : NSObject
@property (nonatomic, copy) NSAttributedString *attributed;
@property (nonatomic) NSPoint origin; // NaN,NaN => centered
@end
@implementation OverlayLabel @end

@interface OverlayRect : NSObject
@property (nonatomic) NSRect rect;
@property (nonatomic) CGFloat strokeWidth;
@property (nonatomic, strong) NSColor *strokeColor;
@property (nonatomic, strong) NSColor *fillColor;
@end
@implementation OverlayRect @end

@interface OverlayView : NSView
@property (atomic, copy) NSArray<OverlayLabel*> *labels;
@property (atomic, copy) NSArray<OverlayRect*> *rects;
@end

@implementation OverlayView
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty {
    [super drawRect:dirty];
    // faint border to confirm overlay is alive (optional)
    CGContextRef ctx = NSGraphicsContext.currentContext.CGContext;
    NSRect r = NSInsetRect(self.bounds, 80, 80);
    CGContextSetLineWidth(ctx, 2);
    CGContextSetStrokeColorWithColor(ctx, [NSColor colorWithWhite:1 alpha:0.12].CGColor);
    CGContextStrokeRect(ctx, r);

    for (OverlayRect *R in self.rects) {
        if (!R) continue;
        CGRect rect = NSRectToCGRect(R.rect);
        if (R.fillColor && R.fillColor.alphaComponent > 0.0) {
            CGContextSetFillColorWithColor(ctx, R.fillColor.CGColor);
            CGContextFillRect(ctx, rect);
        }
        if (R.strokeColor && R.strokeColor.alphaComponent > 0.0 && R.strokeWidth > 0.0) {
            CGContextSetLineWidth(ctx, R.strokeWidth);
            CGContextSetStrokeColorWithColor(ctx, R.strokeColor.CGColor);
            CGContextStrokeRect(ctx, rect);
        }
    }

    for (OverlayLabel *L in self.labels) {
        if (L.attributed.length == 0) continue;
        NSSize sz = L.attributed.size;
        NSPoint p = L.origin;
        if (std::isnan(p.x) || std::isnan(p.y)) {
            p = NSMakePoint(NSMidX(self.bounds) - sz.width/2.0,
                            NSMidY(self.bounds) - sz.height/2.0);
        }
        [L.attributed drawAtPoint:p];
    }
}
@end

static NSMutableArray<NSWindow*> *gWindows;
static NSMutableDictionary<NSString*, OverlayLabel*> *gLabels; // keyed labels
static NSMutableDictionary<NSString*, OverlayRect*> *gRects;   // keyed rectangles

// Back-compat single-text state maps to key @"__default__"
static NSString *kDefaultKey = @"__default__";
static double   gDefaultSize = 48.0;
static NSColor *gDefaultColor;

static NSAttributedString* MakeAttr(NSString *s, double size, NSColor *color) {
    if (!color) color = [NSColor whiteColor];
    NSDictionary *attrs = @{
        NSFontAttributeName: [NSFont boldSystemFontOfSize:(CGFloat)size],
        NSForegroundColorAttributeName: color
    };
    return [[NSAttributedString alloc] initWithString:(s ?: @"") attributes:attrs];
}

static void ApplyLabelsToAllWindows() {
    NSArray *labelVals = gLabels ? gLabels.allValues : @[];
    NSArray *rectVals = gRects ? gRects.allValues : @[];
    for (NSWindow *w in gWindows) {
        OverlayView *v = (OverlayView*)w.contentView;
        v.labels = labelVals;
        v.rects = rectVals;
        [v setNeedsDisplay:YES];
    }
}

static NSWindow* CreateOverlayForScreen(NSScreen *screen) {
    NSRect frame = screen.frame;
    NSWindow *w = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:NSWindowStyleMaskBorderless
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    w.opaque = NO;
    w.backgroundColor = NSColor.clearColor;
    w.hasShadow = NO;
    w.ignoresMouseEvents = YES;
    w.level = NSScreenSaverWindowLevel;
    w.collectionBehavior =
        NSWindowCollectionBehaviorCanJoinAllSpaces |
        NSWindowCollectionBehaviorFullScreenAuxiliary |
        NSWindowCollectionBehaviorIgnoresCycle;

    OverlayView *v = [OverlayView new];
    v.wantsLayer = YES;
    v.layer.backgroundColor = NSColor.clearColor.CGColor;
    v.frame = frame;
    w.contentView = v;

    [w orderFrontRegardless];
    return w;
}

static void CreateOrRecreateWindows() {
    if (!gWindows) gWindows = [NSMutableArray array];
    for (NSWindow *w in gWindows) [w orderOut:nil];
    [gWindows removeAllObjects];
    for (NSScreen *scr in NSScreen.screens) {
        [gWindows addObject:CreateOverlayForScreen(scr)];
    }
    ApplyLabelsToAllWindows();
}

extern "C" void overlay_start() {
    if (![NSThread isMainThread]) { dispatch_sync(dispatch_get_main_queue(), ^{ overlay_start(); }); return; }

    if (NSApp == nil) {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [NSApp finishLaunching];
    }
    if (!gLabels) gLabels = [NSMutableDictionary dictionary];
    if (!gRects) gRects = [NSMutableDictionary dictionary];
    if (!gDefaultColor) gDefaultColor = [NSColor whiteColor];

    CreateOrRecreateWindows();

    [NSNotificationCenter.defaultCenter addObserverForName:NSApplicationDidChangeScreenParametersNotification
                                                    object:nil queue:NSOperationQueue.mainQueue
                                                usingBlock:^(__unused NSNotification *n) {
        CreateOrRecreateWindows();
    }];

    [NSApp activateIgnoringOtherApps:YES];
    for (NSWindow *w in gWindows) [w orderFrontRegardless];
}

extern "C" void overlay_redraw() {
    if (![NSThread isMainThread]) { dispatch_async(dispatch_get_main_queue(), ^{ overlay_redraw(); }); return; }
    ApplyLabelsToAllWindows();
}

extern "C" void overlay_stop() {
    if (![NSThread isMainThread]) { dispatch_sync(dispatch_get_main_queue(), ^{ overlay_stop(); }); return; }
    for (NSWindow *w in gWindows) [w orderOut:nil];
    [gWindows removeAllObjects];
}

extern "C" void overlay_run() {
    if (![NSThread isMainThread]) { dispatch_sync(dispatch_get_main_queue(), ^{ overlay_run(); }); return; }
    if (![NSApp isRunning]) [NSApp run];
}

extern "C" void overlay_step(double seconds) {
    if (![NSThread isMainThread]) { dispatch_sync(dispatch_get_main_queue(), ^{ overlay_step(seconds); }); return; }
    NSDate *until = [NSDate dateWithTimeIntervalSinceNow:seconds];
    NSEvent *e;
    while ((e = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until inMode:NSDefaultRunLoopMode dequeue:YES])) {
        [NSApp sendEvent:e];
    }
    [NSApp updateWindows];
}

// ===== Back-compat single-text API maps to @"__default__" =====
extern "C" void overlay_set_text_utf8(const char* utf8) {
    // IMPORTANT: callers may pass pointers that are only valid for the duration
    // of this call (e.g. std::to_string(...).c_str()). Copy into an NSString
    // before dispatching to the main queue.
    NSString *s = utf8 ? [NSString stringWithUTF8String:utf8] : @"";
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *key = kDefaultKey;
        OverlayLabel *L = gLabels[key] ?: [OverlayLabel new];
        L.attributed = MakeAttr(s, gDefaultSize, gDefaultColor);
        if (!gLabels[key]) { L.origin = (NSPoint){NAN, NAN}; }
        gLabels[key] = L;
        ApplyLabelsToAllWindows();
    });
}
extern "C" void overlay_clear_text() {
    overlay_set_text_utf8("");
}
extern "C" void overlay_set_text_size(double points) {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (points > 0) gDefaultSize = points;
        OverlayLabel *L = gLabels[kDefaultKey];
        if (L) {
            NSString *current = L.attributed.string ?: @"";
            L.attributed = MakeAttr(current, gDefaultSize, gDefaultColor);
            ApplyLabelsToAllWindows();
        }
    });
}
extern "C" void overlay_set_text_color(double r, double g, double b, double a) {
    dispatch_async(dispatch_get_main_queue(), ^{
        CGFloat R = (CGFloat)fmin(fmax(r,0),1);
        CGFloat G = (CGFloat)fmin(fmax(g,0),1);
        CGFloat B = (CGFloat)fmin(fmax(b,0),1);
        CGFloat A = (CGFloat)fmin(fmax(a,0),1);
        gDefaultColor = [NSColor colorWithCalibratedRed:R green:G blue:B alpha:A];
        OverlayLabel *L = gLabels[kDefaultKey];
        if (L) {
            NSString *current = L.attributed.string ?: @"";
            L.attributed = MakeAttr(current, gDefaultSize, gDefaultColor);
            ApplyLabelsToAllWindows();
        }
    });
}
extern "C" void overlay_set_text_position(double x, double y) {
    dispatch_async(dispatch_get_main_queue(), ^{
        OverlayLabel *L = gLabels[kDefaultKey] ?: [OverlayLabel new];
        L.origin = NSMakePoint(x, y); // NaN,NaN => centered
        if (!gLabels[kDefaultKey]) gLabels[kDefaultKey] = L;
        ApplyLabelsToAllWindows();
    });
}

// ===== Multi-label API =====
extern "C" void overlay_label_set(const char* keyC, const char* utf8,
                                  double x, double y, double points,
                                  double r, double g, double b, double a) {
    // Copy inputs before dispatching (callers may pass temporary pointers).
    if (!keyC) return;
    NSString *key = [NSString stringWithUTF8String:keyC];
    NSString *s = utf8 ? [NSString stringWithUTF8String:utf8] : @"";
    dispatch_async(dispatch_get_main_queue(), ^{
        CGFloat R = (CGFloat)fmin(fmax(r,0),1);
        CGFloat G = (CGFloat)fmin(fmax(g,0),1);
        CGFloat B = (CGFloat)fmin(fmax(b,0),1);
        CGFloat A = (CGFloat)fmin(fmax(a,0),1);
        NSColor *color = [NSColor colorWithCalibratedRed:R green:G blue:B alpha:A];
        OverlayLabel *L = gLabels[key] ?: [OverlayLabel new];
        double size = points > 0 ? points : gDefaultSize;
        L.attributed = MakeAttr(s, size, color);
        L.origin = NSMakePoint(x, y); // NaN,NaN centers
        gLabels[key] = L;
        ApplyLabelsToAllWindows();
    });
}

extern "C" void overlay_label_remove(const char* keyC) {
    // Copy key before dispatching (callers may pass temporary pointers).
    if (!keyC) return;
    NSString *key = [NSString stringWithUTF8String:keyC];
    dispatch_async(dispatch_get_main_queue(), ^{
        [gLabels removeObjectForKey:key];
        ApplyLabelsToAllWindows();
    });
}

extern "C" void overlay_labels_clear() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [gLabels removeAllObjects];
        ApplyLabelsToAllWindows();
    });
}

// ===== Rectangle API =====
extern "C" void overlay_rect_set(const char* keyC,
                                  double x, double y, double width, double height,
                                  double strokeWidth,
                                  double strokeR, double strokeG, double strokeB, double strokeA,
                                  double fillR, double fillG, double fillB, double fillA) {
    // Copy key before dispatching (callers may pass temporary pointers).
    if (!keyC) return;
    NSString *key = [NSString stringWithUTF8String:keyC];
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!gRects) gRects = [NSMutableDictionary dictionary];
        OverlayRect *R = gRects[key] ?: [OverlayRect new];

        double normW = width;
        double normH = height;
        double originX = x;
        double originY = y;
        if (normW < 0) {
            originX += normW;
            normW = -normW;
        }
        if (normH < 0) {
            originY += normH;
            normH = -normH;
        }
        R.rect = NSMakeRect((CGFloat)originX, (CGFloat)originY,
                            (CGFloat)normW, (CGFloat)normH);

        CGFloat lineWidth = (CGFloat)fmax(strokeWidth, 0.0);
        R.strokeWidth = lineWidth;

        CGFloat (^ClampComp)(double) = ^CGFloat(double v) {
            return (CGFloat)fmin(fmax(v, 0.0), 1.0);
        };

        CGFloat strokeAlpha = ClampComp(strokeA);
        if (lineWidth > 0.0 && strokeAlpha > 0.0) {
            CGFloat sR = ClampComp(strokeR);
            CGFloat sG = ClampComp(strokeG);
            CGFloat sB = ClampComp(strokeB);
            R.strokeColor = [NSColor colorWithCalibratedRed:sR green:sG blue:sB alpha:strokeAlpha];
        } else {
            R.strokeColor = nil;
        }

        CGFloat fillAlpha = ClampComp(fillA);
        if (fillAlpha > 0.0) {
            CGFloat fR = ClampComp(fillR);
            CGFloat fG = ClampComp(fillG);
            CGFloat fB = ClampComp(fillB);
            R.fillColor = [NSColor colorWithCalibratedRed:fR green:fG blue:fB alpha:fillAlpha];
        } else {
            R.fillColor = nil;
        }

        gRects[key] = R;
        ApplyLabelsToAllWindows();
    });
}

extern "C" void overlay_rect_remove(const char* keyC) {
    // Copy key before dispatching (callers may pass temporary pointers).
    if (!keyC) return;
    NSString *key = [NSString stringWithUTF8String:keyC];
    dispatch_async(dispatch_get_main_queue(), ^{
        [gRects removeObjectForKey:key];
        ApplyLabelsToAllWindows();
    });
}

extern "C" void overlay_rects_clear() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [gRects removeAllObjects];
        ApplyLabelsToAllWindows();
    });
}
