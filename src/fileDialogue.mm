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
#include "globalVars.h"

// patchTypeStrMap[]/patchTypeUserMax (globalVars.h/types.h) are patch-domain names, but the wire
// protocol sends the same category byte for performances too (see project memory: List Names
// protocol) — reused as-is here rather than inventing separate performance category names no
// capture has ever shown to mean anything different.
static NSString * category_name_for(uint8_t category) {
    if (category < patchTypeUserMax) {
        return [NSString stringWithUTF8String:patchTypeStrMap[category]];
    }
    return @"Unknown";
}

// A single left-aligned tab stop so every row's name lands at the same x position regardless of
// how many digits "Bank X, Loc Y:" needs — without this, names drift left/right against each
// other depending on bank/location number width and don't read as a column.
static NSParagraphStyle * bank_location_name_column_style(void) {
    static NSParagraphStyle * style = nil;

    if (!style) {
        NSMutableParagraphStyle * mutableStyle = [[NSMutableParagraphStyle alloc] init];
        NSTextTab *               tab          = [[NSTextTab alloc] initWithType:NSLeftTabStopType location:100];

        [mutableStyle setTabStops:@[tab]];
        style = [mutableStyle copy];
    }
    return style;
}

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

// Row kinds for G2BankLocationListSource's flattened display arrays — a plain entry, a thin
// non-selectable divider (Bank/Location sort: marks where one bank's entries end and the next
// begin), or a non-selectable category header (Category sort: names the group that follows).
typedef NS_ENUM(NSInteger, tBankLocationRowKind) {
    eBankLocationRowNormal,
    eBankLocationRowSeparator,
    eBankLocationRowHeader,
};

// Backs show_bank_location_list_dialogue_async()'s NSTableView — a label-per-row list, no
// selection pre-set (see that function's comment for why: the whole point is not defaulting to
// "row 0" the way an NSPopUpButton silently does). rawName/rawCategoryName/rawBank/rawLocation hold
// one real entry each, in Bank/Location order, as given by the caller; rebuildForSortMode: derives
// the flattened, possibly grouped/reordered display arrays the table view actually reads from.
@interface G2BankLocationListSource : NSObject<NSTableViewDataSource, NSTableViewDelegate>
@property (nonatomic, strong) NSArray<NSString *> * rawNames;
@property (nonatomic, strong) NSArray<NSString *> * rawCategoryNames;
@property (nonatomic, strong) NSArray<NSNumber *> * rawBanks;
@property (nonatomic, strong) NSArray<NSNumber *> * rawLocations;
@property (nonatomic, weak) NSTableView *           tableView;

@property (nonatomic, strong) NSArray<NSString *> * rowLabels;
@property (nonatomic, strong) NSArray<NSNumber *> * rowKinds;
@property (nonatomic, strong) NSArray<NSNumber *> * rowBanks;
@property (nonatomic, strong) NSArray<NSNumber *> * rowLocs;

- (void)rebuildForSortMode:(NSInteger)mode;
@end

@implementation G2BankLocationListSource

// mode: 0 = Bank/Location (the raw order as given), 1 = Category (alphabetical within each
// category, with a header row per group), 2 = fully alphabetical (no grouping).
- (void)rebuildForSortMode:(NSInteger)mode {
    NSUInteger                   count = self.rawNames.count;
    NSMutableArray<NSNumber *> * order = [NSMutableArray arrayWithCapacity:count];

    for (NSUInteger i = 0; i < count; i++) {
        [order addObject:@(i)];
    }

    if (mode == 1) {
        NSArray<NSString *> * categoryNames = self.rawCategoryNames;
        NSArray<NSString *> * names         = self.rawNames;

        [order sortUsingComparator:^NSComparisonResult (NSNumber * a, NSNumber * b) {
             NSComparisonResult catCmp = [categoryNames[a.unsignedIntegerValue] caseInsensitiveCompare:categoryNames[b.unsignedIntegerValue]];

             if (catCmp != NSOrderedSame) {
                 return catCmp;
             }
             return [names[a.unsignedIntegerValue] caseInsensitiveCompare:names[b.unsignedIntegerValue]];
         }];
    } else if (mode == 2) {
        NSArray<NSString *> * names = self.rawNames;

        [order sortUsingComparator:^NSComparisonResult (NSNumber * a, NSNumber * b) {
             return [names[a.unsignedIntegerValue] caseInsensitiveCompare:names[b.unsignedIntegerValue]];
         }];
    }
    // mode == 0: the raw order is already Bank/Location order — nothing to sort.

    NSMutableArray<NSString *> * labels       = [NSMutableArray arrayWithCapacity:count];
    NSMutableArray<NSNumber *> * kinds        = [NSMutableArray arrayWithCapacity:count];
    NSMutableArray<NSNumber *> * banks        = [NSMutableArray arrayWithCapacity:count];
    NSMutableArray<NSNumber *> * locs         = [NSMutableArray arrayWithCapacity:count];
    NSString *                   lastGroupKey = nil;

    for (NSNumber * idxNum in order) {
        NSUInteger idx      = idxNum.unsignedIntegerValue;
        NSString * groupKey = (mode == 0) ? [self.rawBanks[idx] stringValue] : (mode == 1) ? self.rawCategoryNames[idx] : nil;

        if ((groupKey != nil) && ((lastGroupKey == nil) || ![groupKey isEqualToString:lastGroupKey])) {
            if (mode == 0) {
                if (lastGroupKey != nil) {
                    [labels addObject:@""];
                    [kinds addObject:@(eBankLocationRowSeparator)];
                    [banks addObject:@0];
                    [locs addObject:@0];
                }
            } else {
                [labels addObject:self.rawCategoryNames[idx]];
                [kinds addObject:@(eBankLocationRowHeader)];
                [banks addObject:@0];
                [locs addObject:@0];
            }
            lastGroupKey = groupKey;
        }
        [labels addObject:[NSString stringWithFormat:@"Bank %@, Loc %@:\t%@", self.rawBanks[idx], self.rawLocations[idx], self.rawNames[idx]]];
        [kinds addObject:@(eBankLocationRowNormal)];
        [banks addObject:self.rawBanks[idx]];
        [locs addObject:self.rawLocations[idx]];
    }

    self.rowLabels = labels;
    self.rowKinds  = kinds;
    self.rowBanks  = banks;
    self.rowLocs   = locs;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return (NSInteger)self.rowLabels.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    // Reused via makeViewWithIdentifier:owner: rather than alloc'd fresh on every call — handing
    // NSTableView un-pooled one-off views breaks its internal view lifecycle bookkeeping and left
    // dangling nextKeyView links between discarded row views, crashing in
    // -[NSWindow dealloc] -> _recursiveBreakKeyViewLoop -> setNextKeyView: once the alert closed.
    tBankLocationRowKind kind   = (tBankLocationRowKind)[self.rowKinds[(NSUInteger)row] integerValue];

    if (kind == eBankLocationRowSeparator) {
        NSString *const separatorId = @"separator";
        NSBox *         box         = [tableView makeViewWithIdentifier:separatorId owner:self];

        if (!box) {
            box            = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, tableView.bounds.size.width, 1)];
            box.identifier = separatorId;
            [box setBoxType:NSBoxSeparator];
        }
        return box;
    }
    NSString *const      cellId = (kind == eBankLocationRowHeader) ? @"header" : @"label";
    NSTextField *        cell   = [tableView makeViewWithIdentifier:cellId owner:self];

    if (!cell) {
        cell            = [[NSTextField alloc] initWithFrame:NSZeroRect];
        cell.identifier = cellId;
        [cell setBezeled:NO];
        [cell setDrawsBackground:NO];
        [cell setEditable:NO];
        [cell setSelectable:NO];

        if (kind == eBankLocationRowHeader) {
            [cell setFont:[NSFont boldSystemFontOfSize:[NSFont systemFontSize]]];
            [cell setTextColor:[NSColor secondaryLabelColor]];
        }
    }

    if (kind == eBankLocationRowNormal) {
        NSMutableAttributedString * text = [[NSMutableAttributedString alloc] initWithString:self.rowLabels[(NSUInteger)row]];

        [text addAttribute:NSParagraphStyleAttributeName value:bank_location_name_column_style() range:NSMakeRange(0, text.length)];
        [cell setAttributedStringValue:text];
    } else {
        [cell setStringValue:self.rowLabels[(NSUInteger)row]];
    }
    return cell;
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row {
    return ([self.rowKinds[(NSUInteger)row] integerValue] == eBankLocationRowSeparator) ? 9.0 : tableView.rowHeight;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row {
    return [self.rowKinds[(NSUInteger)row] integerValue] == eBankLocationRowNormal;
}

- (void)sortModeChanged:(NSSegmentedControl *)sender {
    [self rebuildForSortMode:sender.selectedSegment];
    [self.tableView reloadData];
}

@end

// A scrollable NSTableView listing pre-built named choices (items), plus a segmented control for
// switching how they're sorted/grouped (see G2BankLocationListSource.rebuildForSortMode: above).
// Unlike an NSPopUpButton (which always shows some item as "currently selected", defaulting to the
// first), the table starts with nothing selected and is fully visible without an extra click to
// expand it; the user must click a row before confirming. Items are copied into Cocoa objects
// synchronously, before this function returns, so the caller's items/name pointers only need to
// survive the call itself.
void show_bank_location_list_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle,
                                            const tBankLocationListItem * items, uint32_t itemCount,
                                            tBankLocationConfirmCallback callback) {
    NSString *                   titleString      = [NSString stringWithUTF8String:(title ? title : "")];
    NSString *                   messageString    = [NSString stringWithUTF8String:(message ? message : "")];
    NSString *                   confirmString    = [NSString stringWithUTF8String:(confirmButtonTitle ? confirmButtonTitle : "OK")];

    // Raw, one-entry-per-item arrays in Bank/Location order — G2BankLocationListSource derives the
    // flattened, possibly grouped/reordered display arrays from these each time the sort mode
    // changes, so nothing here needs to anticipate which sort mode ends up on screen.
    NSMutableArray<NSString *> * rawNames         = [NSMutableArray arrayWithCapacity:itemCount];
    NSMutableArray<NSString *> * rawCategoryNames = [NSMutableArray arrayWithCapacity:itemCount];
    NSMutableArray<NSNumber *> * rawBanks         = [NSMutableArray arrayWithCapacity:itemCount];
    NSMutableArray<NSNumber *> * rawLocations     = [NSMutableArray arrayWithCapacity:itemCount];

    for (uint32_t i = 0; i < itemCount; i++) {
        [rawNames addObject:[NSString stringWithUTF8String:(items[i].name ? items[i].name : "")]];
        [rawCategoryNames addObject:category_name_for(items[i].category)];
        [rawBanks addObject:@(items[i].bank1Indexed)];
        [rawLocations addObject:@(items[i].location1Indexed)];
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
        CGFloat sortControlHeight         = 24;
        CGFloat gap                       = 6;

        NSSegmentedControl * sortControl  = [[NSSegmentedControl alloc] initWithFrame:NSMakeRect(0, listHeight + gap, listWidth, sortControlHeight)];
        [sortControl setSegmentCount:3];
        [sortControl setLabel:@"Bank/Loc" forSegment:0];
        [sortControl setLabel:@"Category" forSegment:1];
        [sortControl setLabel:@"A-Z" forSegment:2];
        [sortControl setSelectedSegment:0];
        [sortControl setSegmentStyle:NSSegmentStyleRounded];

        NSScrollView * scrollView         = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, listWidth, listHeight)];
        NSTableView * tableView           = [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, listWidth, listHeight)];
        NSTableColumn * column            = [[NSTableColumn alloc] initWithIdentifier:@"name"];
        G2BankLocationListSource * source = [[G2BankLocationListSource alloc] init];

        source.rawNames                   = rawNames;
        source.rawCategoryNames           = rawCategoryNames;
        source.rawBanks                   = rawBanks;
        source.rawLocations               = rawLocations;
        source.tableView                  = tableView;
        [source rebuildForSortMode:0];

        [sortControl setTarget:source];
        [sortControl setAction:@selector(sortModeChanged:)];

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

        NSView * accessory                = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, listWidth, listHeight + gap + sortControlHeight)];
        [accessory addSubview:scrollView];
        [accessory addSubview:sortControl];
        [alert setAccessoryView:accessory];
        [[alert window] setInitialFirstResponder:tableView];

        NSModalResponse response          = [alert runModal];
        NSInteger selected                = [tableView selectedRow];
        bool confirmed                    = (response == NSAlertSecondButtonReturn)
                                            && (selected >= 0) && ((NSUInteger)selected < source.rowLabels.count)
                                            && ([source.rowKinds[(NSUInteger)selected] integerValue] == eBankLocationRowNormal);
        uint32_t bank                     = 0;
        uint32_t location                 = 0;

        if (confirmed) {
            bank     = [source.rowBanks[(NSUInteger)selected] unsignedIntValue];
            location = [source.rowLocs[(NSUInteger)selected] unsignedIntValue];
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

