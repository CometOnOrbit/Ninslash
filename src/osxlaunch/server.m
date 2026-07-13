#import <Cocoa/Cocoa.h>

@interface ServerView : NSTextView <NSWindowDelegate>
{
	NSTask *task;
	NSFileHandle *file;
}
- (void)listenTo:(NSTask *)t;
@end

@implementation ServerView
- (void)listenTo:(NSTask *)t
{
	NSPipe *pipe;
	task = t;
	pipe = [NSPipe pipe];
	[task setStandardOutput:pipe];
	file = [pipe fileHandleForReading];

	[[NSNotificationCenter defaultCenter] addObserver:self
		selector:@selector(outputNotification:)
		name:NSFileHandleReadCompletionNotification
		object:file];

	[file readInBackgroundAndNotify];
}

- (void)outputNotification:(NSNotification *)notification
{
	NSData *data = [[notification userInfo] objectForKey:NSFileHandleNotificationDataItem];
	NSString *string = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
	NSAttributedString *attrstr = [[NSAttributedString alloc] initWithString:string];

	[[self textStorage] appendAttributedString:attrstr];
	NSUInteger length = [[self textStorage] length];
	[self scrollRangeToVisible:NSMakeRange(length, 0)];

	[attrstr release];
	[string release];
	[file readInBackgroundAndNotify];
}

- (void)windowWillClose:(NSNotification *)notification
{
	[task terminate];
	[NSApp terminate:self];
}
@end

void runServer()
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSApp = [NSApplication sharedApplication];
	NSBundle *mainBundle = [NSBundle mainBundle];
	NSTask *task = [[NSTask alloc] init];
	[task setCurrentDirectoryPath:[mainBundle resourcePath]];

	NSOpenPanel *openDlg = [NSOpenPanel openPanel];
	[openDlg setCanChooseFiles:YES];
	[openDlg setCanChooseDirectories:NO];
	[openDlg setAllowsMultipleSelection:NO];

	if([openDlg runModal] != NSModalResponseOK)
	{
		[pool release];
		return;
	}

	NSString *filename = [[openDlg URL] path];
	if(!filename)
	{
		[pool release];
		return;
	}

	NSArray *arguments = [NSArray arrayWithObjects:@"-f", filename, nil];
	NSRect graphicsRect = NSMakeRect(100.0, 1000.0, 600.0, 400.0);

	NSWindow *window = [[NSWindow alloc]
		initWithContentRect:graphicsRect
		styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
		backing:NSBackingStoreBuffered
		defer:NO];

	[window setTitle:@"Ninslash Server"];

	ServerView *view = [[[ServerView alloc] initWithFrame:graphicsRect] autorelease];
	[view setEditable:NO];
	[view setRulerVisible:YES];

	[window setContentView:view];
	[window setDelegate:view];
	[window makeKeyAndOrderFront:nil];

	[view listenTo:task];
	[task setLaunchPath:[mainBundle pathForAuxiliaryExecutable:@"ninslash_srv"]];
	[task setArguments:arguments];
	[task launch];
	[NSApp run];
	[task terminate];

	[task release];
	[window release];
	[pool release];
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	runServer();
	return 0;
}
