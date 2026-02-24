#import <Cocoa/Cocoa.h>
#import <Foundation/NSPathUtilities.h>

extern bool release_mode;

// Get the writeable directory for this project
// Ignore the project name in release mode and use the bundle name instead
// Create the directory if it doesn't exist
char* project_writeable(char *project) 
{
  static char ans[1024];
  NSArray* paths, *components;
  NSString* path;
  NSString* bundleid;
  if (release_mode == false) 
    {
      return project;
    }
  paths = NSSearchPathForDirectoriesInDomains (NSLibraryDirectory, 
							NSUserDomainMask, YES);
  bundleid = [[NSBundle mainBundle] bundleIdentifier];
  if (bundleid == nil) bundleid=@"none";
  components = [NSArray arrayWithObjects: [paths objectAtIndex: 0],
				 @"Application Support",
				 bundleid,
				 nil];
  path=[NSString pathWithComponents: components];
  [components release];
  // Make sure the folder exists
  [[NSFileManager defaultManager] createDirectoryAtPath: path
				      attributes: nil];
  return strcpy(ans, [path UTF8String]);
}


