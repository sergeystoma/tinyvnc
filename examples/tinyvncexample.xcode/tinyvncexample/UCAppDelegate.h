//
//  UCAppDelegate.h
//  tinyvncexample
//
//  Created by Sergey Stoma on 11/25/12.
//  Copyright (c) 2012 Under Clouds Studio LLC. All rights reserved.
//

#import <UIKit/UIKit.h>

@class UCViewController;

@interface UCAppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;

@property (strong, nonatomic) UCViewController *viewController;

@end
