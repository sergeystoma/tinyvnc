//
//  UCViewController.h
//  tinyvncexample
//
//  Created by Sergey Stoma on 11/25/12.
//  Copyright (c) 2012 Under Clouds Studio LLC. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface UCViewController : UIViewController

@property (nonatomic,retain) IBOutlet UITextField *txtIp;
@property (nonatomic,retain) IBOutlet UITextField *txtPort;
@property (nonatomic,retain) IBOutlet UITextField *txtUsername;
@property (nonatomic,retain) IBOutlet UITextField *txtPassword;
@property (nonatomic,retain) IBOutlet UITextField *txtKeys;
@property (nonatomic,retain) IBOutlet UIImageView *vwImage;

-(IBAction)grabScreenshot:(id)sender;
-(IBAction)sendKeys:(id)sender;

@end
