/*
 * The G2 Editor application.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#import "fileDialogue.h"
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>

void open_file_read_dialogue_async(tFileDialogueCallback callback) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSOpenPanel * panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setTitle:@"Select a File"];

        [panel beginWithCompletionHandler:^(NSModalResponse result) {
             if (result == NSModalResponseOK) {
                 NSString * path = [panel.URL path];

                 if (callback) {
                     callback([path UTF8String]);
                 }
             } else {
                 if (callback) {
                     callback(NULL);
                 }
             }
         }];
    });
}

void show_alert_async(const char * title, const char * message) {
    NSString * titleString   = [NSString stringWithUTF8String:(title ? title : "")];
    NSString * messageString = [NSString stringWithUTF8String:(message ? message : "")];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert = [[NSAlert alloc] init];

        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    });
}

// Shows a two-button confirm/cancel alert for destructive actions (e.g. Bank Restore, which
// overwrites the target bank on the device). "Cancel" is added first so it's the default
// (Return-key) action — an accidental Enter should never trigger the destructive path.
void show_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle, tConfirmCallback callback) {
    NSString * titleString   = [NSString stringWithUTF8String:(title ? title : "")];
    NSString * messageString = [NSString stringWithUTF8String:(message ? message : "")];
    NSString * confirmString = [NSString stringWithUTF8String:(confirmButtonTitle ? confirmButtonTitle : "OK")];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert          = [[NSAlert alloc] init];

        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:confirmString];

        NSModalResponse response = [alert runModal];

        if (callback) {
            callback(response == NSAlertSecondButtonReturn);
        }
    });
}

// Same as show_confirm_dialogue_async above, plus an accessory NSTextField for picking a target
// bank number that may differ from whatever bank the title/message refer to (Bank Restore's
// "restore this backup into a different bank" case). The field is clamped to
// [1, maxBank1Indexed] via NSNumberFormatter and pre-filled with defaultTargetBank1Indexed.
void show_bank_target_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle,
                                             uint32_t defaultTargetBank1Indexed, uint32_t maxBank1Indexed,
                                             tBankTargetConfirmCallback callback) {
    NSString * titleString   = [NSString stringWithUTF8String:(title ? title : "")];
    NSString * messageString = [NSString stringWithUTF8String:(message ? message : "")];
    NSString * confirmString = [NSString stringWithUTF8String:(confirmButtonTitle ? confirmButtonTitle : "OK")];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert               = [[NSAlert alloc] init];

        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:confirmString];

        NSTextField * label           = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 26, 260, 18)];
        [label setStringValue:@"Restore to Bank:"];
        [label setBezeled:NO];
        [label setDrawsBackground:NO];
        [label setEditable:NO];
        [label setSelectable:NO];

        NSNumberFormatter * formatter = [[NSNumberFormatter alloc] init];
        [formatter setAllowsFloats:NO];
        [formatter setMinimum:@(1)];
        [formatter setMaximum:@(maxBank1Indexed)];

        NSTextField * field           = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 2, 80, 22)];
        [field setFormatter:formatter];
        [field setStringValue:[NSString stringWithFormat:@"%u", defaultTargetBank1Indexed]];

        NSView * accessory            = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 260, 50)];
        [accessory addSubview:label];
        [accessory addSubview:field];
        [alert setAccessoryView:accessory];
        [[alert window] setInitialFirstResponder:field];

        NSModalResponse response      = [alert runModal];
        bool confirmed                = (response == NSAlertSecondButtonReturn);
        uint32_t target               = defaultTargetBank1Indexed;

        if (confirmed) {
            NSInteger value = [field integerValue];

            if (value < 1) {
                value = 1;
            } else if (value > (NSInteger)maxBank1Indexed) {
                value = (NSInteger)maxBank1Indexed;
            }
            target          = (uint32_t)value;
        }

        if (callback) {
            callback(confirmed, target);
        }
    });
}

// Same as show_bank_target_confirm_dialogue_async above, but two independently-clamped accessory
// fields (Bank, Location) stacked vertically — used by Store to Bank's initial "which bank/
// location?" step (the overwrite-warning peek runs after this, once both numbers are known).
void show_bank_location_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle,
                                               uint32_t defaultBank1Indexed, uint32_t maxBank1Indexed,
                                               uint32_t defaultLocation1Indexed, uint32_t maxLocation1Indexed,
                                               tBankLocationConfirmCallback callback) {
    NSString * titleString   = [NSString stringWithUTF8String:(title ? title : "")];
    NSString * messageString = [NSString stringWithUTF8String:(message ? message : "")];
    NSString * confirmString = [NSString stringWithUTF8String:(confirmButtonTitle ? confirmButtonTitle : "OK")];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert                   = [[NSAlert alloc] init];

        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:confirmString];

        // Layout (bottom-up, AppKit's un-flipped coordinate origin): location field [0,22],
        // location label [24,42], bank field [50,72], bank label [74,92] — a 6-8px gap between
        // every element so the label above one field never overlaps the field below the next.
        NSTextField * bankLabel           = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 74, 260, 18)];
        [bankLabel setStringValue:@"Bank:"];
        [bankLabel setBezeled:NO];
        [bankLabel setDrawsBackground:NO];
        [bankLabel setEditable:NO];
        [bankLabel setSelectable:NO];

        NSNumberFormatter * bankFormatter = [[NSNumberFormatter alloc] init];
        [bankFormatter setAllowsFloats:NO];
        [bankFormatter setMinimum:@(1)];
        [bankFormatter setMaximum:@(maxBank1Indexed)];

        NSTextField * bankField           = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 50, 80, 22)];
        [bankField setFormatter:bankFormatter];
        [bankField setStringValue:[NSString stringWithFormat:@"%u", defaultBank1Indexed]];

        NSTextField * locationLabel       = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 24, 260, 18)];
        [locationLabel setStringValue:@"Location:"];
        [locationLabel setBezeled:NO];
        [locationLabel setDrawsBackground:NO];
        [locationLabel setEditable:NO];
        [locationLabel setSelectable:NO];

        NSNumberFormatter * locFormatter  = [[NSNumberFormatter alloc] init];
        [locFormatter setAllowsFloats:NO];
        [locFormatter setMinimum:@(1)];
        [locFormatter setMaximum:@(maxLocation1Indexed)];

        NSTextField * locField            = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 80, 22)];
        [locField setFormatter:locFormatter];
        [locField setStringValue:[NSString stringWithFormat:@"%u", defaultLocation1Indexed]];

        NSView * accessory                = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 260, 92)];
        [accessory addSubview:bankLabel];
        [accessory addSubview:bankField];
        [accessory addSubview:locationLabel];
        [accessory addSubview:locField];
        [alert setAccessoryView:accessory];
        [[alert window] setInitialFirstResponder:bankField];

        NSModalResponse response          = [alert runModal];
        bool confirmed                    = (response == NSAlertSecondButtonReturn);
        uint32_t bank                     = defaultBank1Indexed;
        uint32_t location                 = defaultLocation1Indexed;

        if (confirmed) {
            NSInteger bankValue = [bankField integerValue];
            NSInteger locValue  = [locField integerValue];

            if (bankValue < 1) {
                bankValue = 1;
            } else if (bankValue > (NSInteger)maxBank1Indexed) {
                bankValue = (NSInteger)maxBank1Indexed;
            }

            if (locValue < 1) {
                locValue = 1;
            } else if (locValue > (NSInteger)maxLocation1Indexed) {
                locValue = (NSInteger)maxLocation1Indexed;
            }
            bank                = (uint32_t)bankValue;
            location            = (uint32_t)locValue;
        }

        if (callback) {
            callback(confirmed, bank, location);
        }
    });
}

// Backs show_bank_location_list_dialogue_async()'s NSTableView — a label-per-row list, no
// selection pre-set (see that function's comment for why: the whole point is not defaulting to
// "row 0" the way an NSPopUpButton silently does). Some rows are thin non-selectable dividers
// (rowIsSeparator) marking where one bank's entries end and the next begin — inserted by the
// builder function below, not something the caller specifies directly.
@interface G2BankLocationListSource : NSObject<NSTableViewDataSource, NSTableViewDelegate>
@property (nonatomic, strong) NSArray<NSString *> * rowLabels;
@property (nonatomic, strong) NSArray<NSNumber *> * rowIsSeparator;
@end

@implementation G2BankLocationListSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return (NSInteger)self.rowLabels.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    // Reused via makeViewWithIdentifier:owner: rather than alloc'd fresh on every call — handing
    // NSTableView un-pooled one-off views breaks its internal view lifecycle bookkeeping and left
    // dangling nextKeyView links between discarded row views, crashing in
    // -[NSWindow dealloc] -> _recursiveBreakKeyViewLoop -> setNextKeyView: once the alert closed.
    if ([self.rowIsSeparator[(NSUInteger)row] boolValue]) {
        NSString *const separatorId = @"separator";
        NSBox *         box         = [tableView makeViewWithIdentifier:separatorId owner:self];

        if (!box) {
            box            = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, tableView.bounds.size.width, 1)];
            box.identifier = separatorId;
            [box setBoxType:NSBoxSeparator];
        }
        return box;
    }
    NSString *const cellId = @"label";
    NSTextField *   cell   = [tableView makeViewWithIdentifier:cellId owner:self];

    if (!cell) {
        cell            = [[NSTextField alloc] initWithFrame:NSZeroRect];
        cell.identifier = cellId;
        [cell setBezeled:NO];
        [cell setDrawsBackground:NO];
        [cell setEditable:NO];
        [cell setSelectable:NO];
    }
    [cell setStringValue:self.rowLabels[(NSUInteger)row]];
    return cell;
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row {
    return [self.rowIsSeparator[(NSUInteger)row] boolValue] ? 9.0 : tableView.rowHeight;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row {
    return ![self.rowIsSeparator[(NSUInteger)row] boolValue];
}

@end

// Same idea as show_bank_location_confirm_dialogue_async above, but a scrollable NSTableView
// listing pre-built named choices (items) instead of two blind numeric fields. Unlike an
// NSPopUpButton (which always shows some item as "currently selected", defaulting to the first —
// exactly the "looks like Bank 1/Location 1 is already chosen" behavior this replaced), the table
// starts with nothing selected and is fully visible without an extra click to expand it; the user
// must click a row before confirming. Items are copied into Cocoa objects synchronously, before
// this function returns, so the caller's items/label pointers only need to survive the call itself.
void show_bank_location_list_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle,
                                            const tBankLocationListItem * items, uint32_t itemCount,
                                            tBankLocationConfirmCallback callback) {
    NSString *                   titleString    = [NSString stringWithUTF8String:(title ? title : "")];
    NSString *                   messageString  = [NSString stringWithUTF8String:(message ? message : "")];
    NSString *                   confirmString  = [NSString stringWithUTF8String:(confirmButtonTitle ? confirmButtonTitle : "OK")];

    // Flattened into per-row arrays (not per-item) since a thin separator row is inserted
    // whenever the bank number changes from one item to the next — separators get placeholder
    // bank/location values that are never read (shouldSelectRow: keeps them from ever becoming
    // the selected row).
    NSMutableArray<NSString *> * rowLabels      = [NSMutableArray arrayWithCapacity:itemCount];
    NSMutableArray<NSNumber *> * rowIsSeparator = [NSMutableArray arrayWithCapacity:itemCount];
    NSMutableArray<NSNumber *> * rowBanks       = [NSMutableArray arrayWithCapacity:itemCount];
    NSMutableArray<NSNumber *> * rowLocs        = [NSMutableArray arrayWithCapacity:itemCount];

    for (uint32_t i = 0; i < itemCount; i++) {
        if ((i > 0) && (items[i].bank1Indexed != items[i - 1].bank1Indexed)) {
            [rowLabels addObject:@""];
            [rowIsSeparator addObject:@YES];
            [rowBanks addObject:@0];
            [rowLocs addObject:@0];
        }
        [rowLabels addObject:[NSString stringWithUTF8String:(items[i].label ? items[i].label : "")]];
        [rowIsSeparator addObject:@NO];
        [rowBanks addObject:@(items[i].bank1Indexed)];
        [rowLocs addObject:@(items[i].location1Indexed)];
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert                   = [[NSAlert alloc] init];

        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:confirmString];

        CGFloat listWidth                 = 360;
        CGFloat listHeight                = 220;
        NSScrollView * scrollView         = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, listWidth, listHeight)];
        NSTableView * tableView           = [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, listWidth, listHeight)];
        NSTableColumn * column            = [[NSTableColumn alloc] initWithIdentifier:@"name"];
        G2BankLocationListSource * source = [[G2BankLocationListSource alloc] init];

        source.rowLabels                  = rowLabels;
        source.rowIsSeparator             = rowIsSeparator;

        [column setWidth:listWidth - 4];
        [tableView addTableColumn:column];
        [tableView setHeaderView:nil];
        [tableView setDataSource:source];
        [tableView setDelegate:source];
        [tableView setAllowsEmptySelection:YES];
        [tableView setAllowsMultipleSelection:NO];
        [tableView reloadData];

        [scrollView setDocumentView:tableView];
        [scrollView setHasVerticalScroller:YES];
        [scrollView setAutohidesScrollers:YES];
        [scrollView setBorderType:NSBezelBorder];

        [alert setAccessoryView:scrollView];
        [[alert window] setInitialFirstResponder:tableView];

        NSModalResponse response          = [alert runModal];
        NSInteger selected                = [tableView selectedRow];
        bool confirmed                    = (response == NSAlertSecondButtonReturn)
                                            && (selected >= 0) && ((NSUInteger)selected < rowLabels.count)
                                            && !([rowIsSeparator[(NSUInteger)selected] boolValue]);
        uint32_t bank                     = 0;
        uint32_t location                 = 0;

        if (confirmed) {
            bank     = [rowBanks[(NSUInteger)selected] unsignedIntValue];
            location = [rowLocs[(NSUInteger)selected] unsignedIntValue];
        }

        if (callback) {
            callback(confirmed, bank, location);
        }
    });
}

void open_folder_dialogue_async(tFileDialogueCallback callback, const char * title) {
    NSString * titleString = (title && title[0] != '\0')
                            ? [NSString stringWithUTF8String:title]
                            : @"Select a Folder";

    dispatch_async(dispatch_get_main_queue(), ^{
        NSOpenPanel * panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:NO];
        [panel setCanChooseDirectories:YES];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanCreateDirectories:YES];
        [panel setTitle:titleString];

        [panel beginWithCompletionHandler:^(NSModalResponse result) {
             if (result == NSModalResponseOK) {
                 NSString * path = [panel.URL path];

                 if (callback) {
                     callback([path UTF8String]);
                 }
             } else {
                 if (callback) {
                     callback(NULL);
                 }
             }
         }];
    });
}

void open_file_write_dialogue_async(tFileDialogueCallback callback, const char * defaultName) {
    // Capture defaultName before dispatching — it may be stack-allocated
    NSString * nameString = (defaultName && defaultName[0] != '\0')
                          ? [NSString stringWithUTF8String:defaultName]
                          : @"patch.pch2";

    dispatch_async(dispatch_get_main_queue(), ^{
        NSSavePanel * panel = [NSSavePanel savePanel];
        [panel setTitle:@"Save File As"];
        [panel setPrompt:@"Save"];
        [panel setCanCreateDirectories:YES];
        [panel setNameFieldStringValue:nameString];
        [panel setMessage:@"Choose where to save your file."];
        [panel setShowsTagField:NO];
        [panel setExtensionHidden:NO];

        [panel beginWithCompletionHandler:^(NSModalResponse result) {
             if (result == NSModalResponseOK) {
                 NSString * path = panel.URL.path;

                 if (callback) {
                     callback([path UTF8String]);
                 }
             } else {
                 if (callback) {
                     callback(NULL);
                 }
             }
         }];
    });
}

