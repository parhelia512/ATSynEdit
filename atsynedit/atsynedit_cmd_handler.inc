{$ifdef none}begin end;{$endif}

procedure TATSynEdit.DoMenuContextFromKeyboard;
var
  Caret: TATCaretItem;
  Pnt: TPoint;
  PntA: TATPoint;
begin
  if Carets.Count=0 then exit;
  Caret:= Carets[0];
  PntA:= CaretPosToClientPos(Point(Caret.PosX, Caret.PosY));
  //caret out of screen? don't show menu.
  if PntA.Y<0 then exit;
  if PntA.Y>Height then exit;

  Pnt:= ClientToScreen(Point(PntA.X, PntA.Y));

  if Assigned(FMenuText) then
    FMenuText.PopUp(Pnt.X, Pnt.Y)
  else
  begin
    InitMenuStd;
    FMenuStd.PopUp(Pnt.X, Pnt.Y);
  end;
end;

procedure TATSynEdit.KeyDown(var Key: Word; Shift: TShiftState);
var
  NCmd: integer;
begin
  inherited;

  case Key of
    0:
      exit;
    VK_CONTROL,
    VK_LWIN,
    VK_RWIN:
      begin
        if FMouseDragDropping then
          UpdateCursor;
        exit;
      end;
    VK_SHIFT,
    VK_MENU, //3 usual state keys
    VK_LSHIFT..VK_RMENU: //also 6 state keys
      begin
        //ignore Alt/Shift to not hide caret
        exit;
      end;
    VK_TAB:
      begin
        if not WantTabs then exit;
      end;
    VK_RETURN:
      begin
        if not WantReturns then exit;
      end;
    VK_APPS:
      begin
        DoMenuContextFromKeyboard;
        exit;
      end;
  end;

  NCmd:= GetCommandFromKey(Key, Shift);
  if NCmd>0 then
  begin
    TimerBlinkDisable;
    DoCommand(NCmd, TATCommandInvoke.Hotkey);
    TimerBlinkEnable;
  end;
end;

procedure TATSynEdit.KeyUp(var Key: Word; Shift: TShiftState);
begin
  inherited;
  case Key of
    VK_CONTROL,
    VK_LWIN,
    VK_RWIN:
      begin
        if FMouseDragDropping then
          UpdateCursor;
        exit;
      end;
  end;
end;

class function TATSynEdit.CheckInputForNumberOnly(const S: UnicodeString; X: integer; ch: WideChar; AllowNegative: boolean): boolean;
var
  bMinusPresent: boolean;
begin
  Result:= false;
  bMinusPresent:= Pos('-', S)>0;
  case ch of
    '-':
      begin
        if not AllowNegative then exit(false);
        if S='' then
          exit(true);
        if (X=0) and not bMinusPresent then
          exit(true);
        exit(false);
      end;
  '0'..'9':
    begin
      if S='' then
        exit(true);
      if (X=0) and bMinusPresent then
        exit(false);
      exit(true);
    end;
  end;
end;

procedure TATSynEdit.UTF8KeyPress(var UTF8Key: TUTF8Char);
var
  Str, StrLine: atString;
  Allow: boolean;
  Caret: TATCaretItem;
begin
  inherited;

  //skip control Ascii chars
  if Ord(UTF8Key[1])<32 then Exit;

  Str:= Utf8Decode(Utf8Key);
  if Str='' then Exit;

  if Assigned(FOnCheckInput) then
  begin
    Allow:= true;
    FOnCheckInput(Self, Str[1], Allow);
    if not Allow then Exit;
  end;

  if FOptInputNumberOnly then
  begin
    if Length(Str)<>1 then exit;
    if Carets.Count<>1 then exit;
    Caret:= Carets[0];
    if not Strings.IsIndexValid(Caret.PosY) then exit;
    StrLine:= Strings.Lines[Caret.PosY];
    if Caret.PosX>Length(StrLine) then exit;
    if not CheckInputForNumberOnly(
      StrLine,
      Caret.PosX,
      Str[1],
      FOptInputNumberAllowNegative) then exit;
  end;

  TimerBlinkDisable;
  DoCommand(cCommand_TextInsert, TATCommandInvoke.HotkeyChar, Str);
  TimerBlinkEnable;

  Utf8Key:= '';
end;

function TATSynEdit.GetCommandFromKey(var Key: Word; Shift: TShiftState): integer;
var
  Shortcut: TShortcut;
  bComboPressing: boolean;
begin
  Result:= 0;

  if (Key=VK_TAB) and (Shift=[]) then
  begin
    Result:= cCommand_KeyTab;
    Key:= 0;
    Exit;
  end;

  //support combo-hotkeys with simple letter
  //CudaText issue #2690
  bComboPressing:= (FKeymapHistory.Length>0);
  if not bComboPressing then
    if IsEditorHotkeyForText(Key) and ((Shift=[]) or (Shift=[ssShift])) then
      Exit;

  Shortcut:= KeyToShortCut(Key, Shift);
  Result:= FKeymap.GetCommandFromShortcut(Shortcut, FKeymapHistory);
  if Result>0 then
    Key:= 0;
end;

function TATSynEdit.IsCommandResults_CaretMove(Res: TATCommandResults): boolean;
begin
  Result:= Res*[
    TATCommandResult.CaretAny,
    TATCommandResult.CaretLeft,
    TATCommandResult.CaretRight,
    TATCommandResult.CaretTop,
    TATCommandResult.CaretBottom
    ] <> [];
end;

procedure TATSynEdit.DoCommandResults(ACmd: integer; Res: TATCommandResults);
var
  Caret: TATCaretItem;
  St: TATStrings;
  bTextChanged, bCaretMoved, bUndoRedo, bFoldChanged: boolean;
  bNeedRepaint, bAllowRepaint, bAllowCaretScroll: boolean;
  NLineChanged: integer;
  NProximityVert: integer;
begin
  St:= Strings;
  bNeedRepaint:= false;
  bAllowRepaint:= true;
  bTextChanged:= TATCommandResult.Text in Res;
  bUndoRedo:= TATCommandResult.UndoRedo in Res;
  bFoldChanged:= TATCommandResult.FoldChange in Res;
  bCaretMoved:= IsCommandResults_CaretMove(Res);

  //if one-line, try to make right edge of text near the edge of control, like Win inputs
  if ModeOneLine then
    if bTextChanged then
    begin
      FScrollHorz.SetZero;
      Include(Res, TATCommandResult.CaretTop);
    end;

  //prevent bad effects when user edits a line with "fold_style":4, unfold the edited line
  if bTextChanged then
    DoUnfoldLinesWithCarets;

  if bTextChanged or bFoldChanged then
  begin
    bNeedRepaint:= true;
    FWrapUpdateNeeded:= true;
    UpdateWrapInfo;
  end;

  if bTextChanged or St.ModifiedRecent then
    St.ActionSaveLastEditionPos; //clears St.ModifiedRecent

  if bTextChanged then
  begin
    bAllowRepaint:= FOptAllowRepaintOnTextChange;
    if FOptMouseHideCursor then
      if PtInRect(FRectMain, ScreenToClient(Mouse.CursorPos)) then
        SetCursor(crNone);
  end;

  case ACmd of
    cCommand_KeyLeft,
    cCommand_KeyLeft_Sel,
    cCommand_CaretsExtendDownLine..cCommand_CaretsExtendUpToTop:
      DoCaretsFixForSurrogatePairs(false);
    cCommand_KeyRight,
    cCommand_KeyRight_Sel:
      DoCaretsFixForSurrogatePairs(true);
  end;

  DoCaretsFixIfInsideFolded;
  Carets.Sort;

  if bCaretMoved then
  begin
    if Carets.Count=1 then
      bAllowCaretScroll:= true
    else
    if TATCommandResult.CaretLazy in Res then
      bAllowCaretScroll:= not IsCaretOnVisibleRect
    else
      bAllowCaretScroll:= true;

    if bAllowCaretScroll then
    begin
      if TATCommandResult.CaretTop in Res then
        DoGotoCaret(TATCaretEdge.Top, bUndoRedo, false, false, false)
      else
      if TATCommandResult.CaretBottom in Res then
        DoGotoCaret(TATCaretEdge.Bottom, bUndoRedo, false, false, false)
      else
      if TATCommandResult.CaretLeft in Res then
        DoGotoCaret(TATCaretEdge.Left, bUndoRedo, false, false, false)
      else
      if TATCommandResult.CaretRight in Res then
        DoGotoCaret(TATCaretEdge.Right, bUndoRedo, false, false, false);
    end;

    if not (TATCommandResult.KeepColumnSel in Res) then
      FSelRect:= cRectEmpty;

    //update pixel coords, so that CudaText python code
    //    for in in range(N):
    //        ed.cmd(cCommand_KeyDown)
    //will move-down exactly N times
    UpdateCaretsCoords;


    NProximityVert:= GetActualProximityVert;

    //if user clicked near the edge (distance_from_edge<proximity), decrease the proximity
    if (FCaretDistanceFromTop>=0) and
      (FCaretDistanceFromTop<NProximityVert) then
      NProximityVert:= FCaretDistanceFromTop;
    if (FCaretDistanceFromBottom>=0) and
      (FCaretDistanceFromBottom<NProximityVert) then
      NProximityVert:= FCaretDistanceFromBottom;

    if (NProximityVert>0)
      and (ACmd<>cCommand_SelectAll) //don't apply Proximity if 'select all' is called
      and (
        (FCaretDistanceFromTop>=NProximityVert) and
        (FCaretDistanceFromBottom>=NProximityVert)
        ) //don't apply Proximity if caret was near the edge, before the command
      then
    begin
      if (Carets.Count=1) or (Res*[TATCommandResult.CaretTop, TATCommandResult.CaretAny]<>[]) then
      begin
        Caret:= Carets[0];
        if DoCaretApplyProximityToVertEdge(
             Point(Caret.PosX, Caret.PosY),
             Caret.CoordY,
             NProximityVert,
             NProximityVert) then
          UpdateCaretsCoords(false, true);
      end
      else
      if (Carets.Count>1) and (Res*[TATCommandResult.CaretBottom]<>[]) then
      begin
        Caret:= Carets[Carets.Count-1];
        if DoCaretApplyProximityToVertEdge(
             Point(Caret.PosX, Caret.PosY),
             Caret.CoordY,
             NProximityVert,
             NProximityVert) then
          UpdateCaretsCoords(false, true);
      end;
    end;
  end
  else
  //scroll should do UpdateCaretsCoords, CudaText issue #3777
  if TATCommandResult.Scroll in Res then
    UpdateCaretsCoords(false, true);


  if bTextChanged then
  begin
    if not (TATCommandResult.KeepColumnSel in Res) then
      FSelRect:= cRectEmpty;
    //fixing CudaText issue #3708, syntax highlight is broken after Undo,
    //with "Commands History" plugin (cannot find how plugin breaks it)
    if bUndoRedo and (Carets.Count>0) then
      NLineChanged:= Carets[0].FirstTouchedLine
    else
      NLineChanged:= -1;
    DoEventChange(NLineChanged);
  end;

  //fire DoEventCarets after DoEventChange, so that 'highlight pair brackets' properly works
  if bCaretMoved then
    DoEventCarets;

  if TATCommandResult.State in Res then
    DoEventState;

  //when caret goes to left/top edge, must reset the NPixelOffset
  if FOptScrollSmooth and (Carets.Count=1) then
  begin
    Caret:= Carets[0];
    if FScrollVert.NPixelOffset>0 then
      if Caret.CoordY=FRectMain.Top then
        FScrollVert.NPixelOffset:= 0;
    if FScrollHorz.NPixelOffset>0 then
      if Caret.CoordX=FRectMain.Left then
        FScrollHorz.NPixelOffset:= 0;
  end;

  if bNeedRepaint then
  begin
    //- don't call Update, because it does unneeded Invalidate (we call Paint below so Invalidate not needed)
    //- Paint call needed, to repaint after "Fold all" (Linux gtk2)
    //and it helps to not skip repaints in big files with EControl lexer
    if bFoldChanged then
    begin
      Paint;
      //lines before caret can be folded, so caret can go out of screen
      DoGotoCaret(TATCaretEdge.Top, bUndoRedo, false, false, true);
    end;
    if bAllowRepaint then
      Invalidate;
  end
  else
  begin
    if bAllowRepaint then
      Invalidate;
  end;
end;

function TATSynEdit.DoCommandCore(ACmd: integer; const AText: atString): TATCommandResults;
begin
  Result:= [];
  case ACmd of
    //most used commands

    //Shift+arrows must make column selection, if column select option on
    cCommand_KeyLeft:
      Result:= DoCommand_KeyLeft(false);
    cCommand_KeyLeft_Sel:
      begin
        {
        if OptMouseColumnSelectionWithoutKey then
          Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.Left)
        else
          Result:= DoCommand_KeyLeft(true);
          }
         OptMouseColumnSelectionWithoutKey:= false;
         Result:= DoCommand_KeyLeft(true);
       end;

    cCommand_KeyRight:
      Result:= DoCommand_KeyRight(false);
    cCommand_KeyRight_Sel:
      begin
        {
        if OptMouseColumnSelectionWithoutKey then
          Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.Right)
        else
          Result:= DoCommand_KeyRight(true);
          }
        OptMouseColumnSelectionWithoutKey:= false;
        Result:= DoCommand_KeyRight(true);
      end;

    cCommand_KeyUp:
      Result:= DoCommand_KeyUpDown(false, 1, false, false);
    cCommand_KeyUp_Sel:
      begin
        {
        if OptMouseColumnSelectionWithoutKey then
          Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.Up)
        else
          Result:= DoCommand_KeyUpDown(false, 1, false);
          }
        OptMouseColumnSelectionWithoutKey:= false;
        Result:= DoCommand_KeyUpDown(false, 1, false, true);
      end;

    cCommand_KeyDown:
      Result:= DoCommand_KeyUpDown(true, 1, false, false);
    cCommand_KeyDown_Sel:
      begin
        {
        if OptMouseColumnSelectionWithoutKey then
          Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.Down)
        else
          Result:= DoCommand_KeyUpDown(true, 1, false);
          }
        OptMouseColumnSelectionWithoutKey:= false;
        Result:= DoCommand_KeyUpDown(true, 1, false, true);
      end;

    cCommand_KeyHome:
      Result:= DoCommand_KeyHome;
    cCommand_KeyHome_Sel:
      begin
        {
        if OptMouseColumnSelectionWithoutKey then
          Result:= DoCommand_SelectColumnToLineEdge(false)
        else
          Result:= DoCommand_KeyHome;
          }
        OptMouseColumnSelectionWithoutKey:= false;
        Result:= DoCommand_KeyHome;
      end;

    cCommand_KeyEnd:
      Result:= DoCommand_KeyEnd;
    cCommand_KeyEnd_Sel:
      begin
        {
        if OptMouseColumnSelectionWithoutKey then
          Result:= DoCommand_SelectColumnToLineEdge(true)
        else
          Result:= DoCommand_KeyEnd;
          }
        OptMouseColumnSelectionWithoutKey:= false;
        Result:= DoCommand_KeyEnd;
      end;

    cCommand_KeyPageUp:
      Result:= DoCommand_KeyUpDown(false, GetPageLines, FOptKeyPageKeepsRelativePos, false);
    cCommand_KeyPageUp_Sel:
      begin
        {
        if OptMouseColumnSelectionWithoutKey then
          Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.PageUp)
        else
          Result:= DoCommand_KeyUpDown(false, GetPageLines, FOptKeyPageKeepsRelativePos);
          }
        OptMouseColumnSelectionWithoutKey:= false;
        Result:= DoCommand_KeyUpDown(false, GetPageLines, FOptKeyPageKeepsRelativePos, true);
      end;

    cCommand_KeyPageDown:
      Result:= DoCommand_KeyUpDown(true, GetPageLines, FOptKeyPageKeepsRelativePos, false);
    cCommand_KeyPageDown_Sel:
      begin
        {
        if OptMouseColumnSelectionWithoutKey then
          Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.PageDown)
        else
          Result:= DoCommand_KeyUpDown(true, GetPageLines, FOptKeyPageKeepsRelativePos);
          }
        OptMouseColumnSelectionWithoutKey:= false;
        Result:= DoCommand_KeyUpDown(true, GetPageLines, FOptKeyPageKeepsRelativePos, true);
      end;

    cCommand_ColSelectLeft:        Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.Left);
    cCommand_ColSelectRight:       Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.Right);
    cCommand_ColSelectUp:          Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.Up);
    cCommand_ColSelectDown:        Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.Down);
    cCommand_ColSelectPageUp:      Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.PageUp);
    cCommand_ColSelectPageDown:    Result:= DoCommand_SelectColumnToDirection(TATEditorSelectColumnDirection.PageDown);
    cCommand_ColSelectToLineBegin: Result:= DoCommand_SelectColumnToLineEdge(false);
    cCommand_ColSelectToLineEnd:   Result:= DoCommand_SelectColumnToLineEdge(true);

    cCommand_ColSelectWithoutKey_On:
      Result:= DoCommand_ColumnSelectWithoutKey(true);
    cCommand_ColSelectWithoutKey_Off:
      Result:= DoCommand_ColumnSelectWithoutKey(false);
    cCommand_ColSelectWithoutKey_Toggle:
      begin
        OptMouseColumnSelectionWithoutKey:= not OptMouseColumnSelectionWithoutKey;
        Result:= [TATCommandResult.State];
      end;

    cCommand_RemoveFirstCaret:    Result:= DoCommand_RemoveOneCaret(true);
    cCommand_RemoveLastCaret:     Result:= DoCommand_RemoveOneCaret(false);

    cCommand_TextInsert:          Result:= DoCommand_TextInsertAtCarets(AText, false, FOverwrite, false, false);
    cCommand_TextInsertTabChar:   Result:= DoCommand_TextInsertAtCarets(#9, false, FOverwrite, false, false);
    cCommand_KeyBackspace:        Result:= DoCommand_TextBackspace;
    cCommand_KeyDelete:           Result:= DoCommand_TextDelete;
    cCommand_KeyTab:              Result:= DoCommand_TextTabulation;
    cCommand_KeyEnter:            Result:= DoCommand_TextInsertEol(false);
    cCommand_ForceFinalEndOfLine: Result:= DoCommand_ForceFinalEndOfLine;
    cCommand_DeleteFinalEndOfLine: Result:= DoCommand_DeleteFinalEndOfLine;

    cCommand_Undo:                Result:= DoCommand_UndoRedo(true);
    cCommand_Redo:                Result:= DoCommand_UndoRedo(false);
    //end of most used

    cCommand_TextDeleteSelection:      Result:= DoCommand_TextDeleteSelection;
    cCommand_TextDeleteLine:           Result:= DoCommand_TextDeleteLines;
    cCommand_TextDuplicateLine:        Result:= DoCommand_TextDuplicateLine;
    cCommand_TextDeleteToLineBegin:    Result:= DoCommand_TextDeleteToLineBegin;
    cCommand_TextDeleteToLineEnd:      Result:= DoCommand_TextDeleteToLineEnd;
    cCommand_TextDeleteToTextBegin:    Result:= DoCommand_TextDeleteToDocumentBegin;
    cCommand_TextDeleteToTextEnd:      Result:= DoCommand_TextDeleteToDocumentEnd;
    cCommand_TextDeleteWordPrev:       Result:= DoCommand_TextDeleteWord(false);
    cCommand_TextDeleteWordNext:       Result:= DoCommand_TextDeleteWord(true);
    cCommand_TextDeleteWordEntire:     Result:= DoCommand_TextDeleteWordEntire;

    cCommand_SelectAll:                Result:= DoCommand_SelectAll;
    cCommand_SelectNone:               Result:= [TATCommandResult.CaretAny];
    cCommand_SelectInverted:           Result:= DoCommand_SelectInverted;
    cCommand_SelectSplitToLines:       Result:= DoCommand_SelectSplitToLines;
    cCommand_SelectExtendByLine:       Result:= DoCommand_SelectExtendByLine(false);
    cCommand_SelectExtendByLineUp:     Result:= DoCommand_SelectExtendByLine(true);
    cCommand_SelectWords:              Result:= DoCommand_SelectWords;
    cCommand_SelectLines:              Result:= DoCommand_SelectLines;

    cCommand_GotoTextBegin,
    cCommand_GotoTextBegin_Sel:        Result:= DoCommand_GotoTextBegin;
    cCommand_GotoTextEnd,
    cCommand_GotoTextEnd_Sel:          Result:= DoCommand_GotoTextEnd;

    cCommand_GotoScreenTop:            Result:= DoCommand_GotoScreenSide(TATCaretScreenSide.Top);
    cCommand_GotoScreenBottom:         Result:= DoCommand_GotoScreenSide(TATCaretScreenSide.Bottom);
    cCommand_GotoScreenCenter:         Result:= DoCommand_GotoScreenSide(TATCaretScreenSide.Middle);

    cCommand_GotoWordNext,
    cCommand_GotoWordNext_Sel:         Result:= DoCommand_GotoWord(TATWordJump.Next);
    cCommand_GotoWordPrev,
    cCommand_GotoWordPrev_Sel:         Result:= DoCommand_GotoWord(TATWordJump.Prev);

    cCommand_GotoWordNext_Simple,
    cCommand_GotoWordNext_Simple_Sel:  Result:= DoCommand_GotoWord(TATWordJump.Next, true);
    cCommand_GotoWordPrev_Simple,
    cCommand_GotoWordPrev_Simple_Sel:  Result:= DoCommand_GotoWord(TATWordJump.Prev, true);

    cCommand_GotoWordEnd,
    cCommand_GotoWordEnd_Sel:          Result:= DoCommand_GotoWord(TATWordJump.EndOrNext);

    cCommand_GotoLineAbsBegin,
    cCommand_GotoLineAbsBegin_Sel:     Result:= DoCommand_GotoLineEdge(true);
    cCommand_GotoLineAbsEnd,
    cCommand_GotoLineAbsEnd_Sel:       Result:= DoCommand_GotoLineEdge(false);

    cCommand_ToggleOverwrite:          Result:= DoCommand_ToggleOverwrite;
    cCommand_ToggleReadOnly:           Result:= DoCommand_ToggleReadOnly;
    cCommand_ToggleWordWrap:           Result:= DoCommand_ToggleWordWrap(false);
    cCommand_ToggleWordWrapAlt:        Result:= DoCommand_ToggleWordWrap(true);
    cCommand_ToggleUnprinted:          Result:= DoCommand_ToggleUnprinted;
    cCommand_ToggleUnprintedSpaces:    Result:= DoCommand_ToggleUnprintedSpaces;
    cCommand_ToggleUnprintedSpacesTrailing: Result:= DoCommand_ToggleUnprintedSpacesTrailing;
    cCommand_ToggleUnprintedEnds:      Result:= DoCommand_ToggleUnprintedEnds;
    cCommand_ToggleUnprintedEndDetails: Result:= DoCommand_ToggleUnprintedEndDetails;
    cCommand_ToggleLineNums:           Result:= DoCommand_ToggleLineNums;
    cCommand_ToggleFolding:            Result:= DoCommand_ToggleFolding;
    cCommand_ToggleRuler:              Result:= DoCommand_ToggleRuler;
    cCommand_ToggleMinimap:            Result:= DoCommand_ToggleMiniMap;
    cCommand_ToggleMicromap:           Result:= DoCommand_ToggleMicroMap;

    cCommand_TextIndent:               Result:= DoCommand_TextIndentUnindent(true);
    cCommand_TextUnindent:             Result:= DoCommand_TextIndentUnindent(false);

    cCommand_ScrollToBegin:            Result:= DoCommand_ScrollToBeginOrEnd(true, FOptScrollLineCommandsKeepCaretOnScreen);
    cCommand_ScrollToEnd:              Result:= DoCommand_ScrollToBeginOrEnd(false, FOptScrollLineCommandsKeepCaretOnScreen);
    cCommand_ScrollLineUp:             Result:= DoCommand_ScrollByDelta(-1, 0, FOptScrollLineCommandsKeepCaretOnScreen);
    cCommand_ScrollLineDown:           Result:= DoCommand_ScrollByDelta(1, 0, FOptScrollLineCommandsKeepCaretOnScreen);
    cCommand_ScrollPageUp:             Result:= DoCommand_ScrollByDelta(-GetPageLines, 0, FOptScrollLineCommandsKeepCaretOnScreen);
    cCommand_ScrollPageDown:           Result:= DoCommand_ScrollByDelta(GetPageLines, 0, FOptScrollLineCommandsKeepCaretOnScreen);
    cCommand_ScrollColumnLeft:         Result:= DoCommand_ScrollByDelta(0, -1, FOptScrollLineCommandsKeepCaretOnScreen);
    cCommand_ScrollColumnRight:        Result:= DoCommand_ScrollByDelta(0, 1, FOptScrollLineCommandsKeepCaretOnScreen);
    cCommand_ScrollToCaretTop:         Result:= [TATCommandResult.CaretTop];
    cCommand_ScrollToCaretBottom:      Result:= [TATCommandResult.CaretBottom];
    cCommand_ScrollToCaretLeft:        Result:= [TATCommandResult.CaretLeft];
    cCommand_ScrollToCaretRight:       Result:= [TATCommandResult.CaretRight];
    cCommand_ScrollToLeft:             Result:= DoCommand_ScrollToLeft;

    cCommand_Sort_Asc:                 Result:= DoCommand_Sort(TATStringsSortAction.Asc);
    cCommand_Sort_AscNoCase:           Result:= DoCommand_Sort(TATStringsSortAction.AscNoCase);
    cCommand_Sort_Desc:                Result:= DoCommand_Sort(TATStringsSortAction.Desc);
    cCommand_Sort_DescNoCase:          Result:= DoCommand_Sort(TATStringsSortAction.DescNoCase);

    cCommand_DeleteAllBlanks:          Result:= DoCommand_DeleteAllBlanks;
    cCommand_DeleteAdjacentBlanks:     Result:= DoCommand_DeleteAdjacentBlanks;
    cCommand_DeleteAdjacentDups:       Result:= DoCommand_DeleteAdjacentDups;
    cCommand_DeleteAllDups:            Result:= DoCommand_DeleteAllDups(false);
    cCommand_DeleteAllDupsKeepBlanks:  Result:= DoCommand_DeleteAllDups(true);
    cCommand_ReverseLines:             Result:= DoCommand_ReverseLines;
    cCommand_ShuffleLines:             Result:= DoCommand_ShuffleLines;

    cCommand_ClipboardCopy:            Result:= DoCommand_ClipboardCopy(false, Clipboard);
    cCommand_ClipboardCopyAdd:         Result:= DoCommand_ClipboardCopy(true, Clipboard);
    cCommand_ClipboardCut:             Result:= DoCommand_ClipboardCut(Clipboard);

    //use Clipboard:TClipboard
    cCommand_ClipboardPaste:                 Result:= DoCommand_ClipboardPaste(false, false,
                                                        Clipboard.HasFormat(ATEditorOptions.ClipboardColumnFormat),
                                                        Clipboard.AsText);
    cCommand_ClipboardPaste_Select:          Result:= DoCommand_ClipboardPaste(false, true,
                                                        Clipboard.HasFormat(ATEditorOptions.ClipboardColumnFormat),
                                                        Clipboard.AsText);
    cCommand_ClipboardPaste_KeepCaret:       Result:= DoCommand_ClipboardPaste(true, false,
                                                        Clipboard.HasFormat(ATEditorOptions.ClipboardColumnFormat),
                                                        Clipboard.AsText);
    cCommand_ClipboardPaste_Column:          Result:= DoCommand_ClipboardPasteColumnBlock(false,
                                                        Clipboard.AsText);
    cCommand_ClipboardPaste_ColumnKeepCaret: Result:= DoCommand_ClipboardPasteColumnBlock(true,
                                                        Clipboard.AsText);

    cCommand_ClipboardPasteAndIndent:        Result:= DoCommand_ClipboardPasteAndIndent;
    cCommand_ClipboardPasteFromRecents:      Result:= DoCommand_ClipboardPasteFromRecents;
    cCommand_ClipboardClearRecents:          Result:= DoCommand_ClipboardClearRecents;

    //use PrimarySelection:TClipboard
    cCommand_ClipboardAltPaste:                 Result:= DoCommand_ClipboardPaste(false, false,
                                                           PrimarySelection.HasFormat(ATEditorOptions.ClipboardColumnFormat),
                                                           PrimarySelection.AsText);
    cCommand_ClipboardAltPaste_Select:          Result:= DoCommand_ClipboardPaste(false, true,
                                                           PrimarySelection.HasFormat(ATEditorOptions.ClipboardColumnFormat),
                                                           PrimarySelection.AsText);
    cCommand_ClipboardAltPaste_KeepCaret:       Result:= DoCommand_ClipboardPaste(true, false,
                                                           PrimarySelection.HasFormat(ATEditorOptions.ClipboardColumnFormat),
                                                           PrimarySelection.AsText);
    cCommand_ClipboardAltPaste_Column:          Result:= DoCommand_ClipboardPasteColumnBlock(false,
                                                           PrimarySelection.AsText);
    cCommand_ClipboardAltPaste_ColumnKeepCaret: Result:= DoCommand_ClipboardPasteColumnBlock(true,
                                                           PrimarySelection.AsText);
    //use SecondarySelection:TClipboard
    cCommand_ClipboardAltAltPaste:              Result:= DoCommand_ClipboardPaste(false, false,
                                                           SecondarySelection.HasFormat(ATEditorOptions.ClipboardColumnFormat),
                                                           SecondarySelection.AsText);

    cCommand_MoveSelectionUp:          Result:= DoCommand_MoveSelectionUpDown(false);
    cCommand_MoveSelectiondown:        Result:= DoCommand_MoveSelectionUpDown(true);
    cCommand_TextInsertEmptyAbove:     Result:= DoCommand_TextInsertEmptyAboveBelow(false);
    cCommand_TextInsertEmptyBelow:     Result:= DoCommand_TextInsertEmptyAboveBelow(true);

    cCommand_TextCaseLower:            Result:= DoCommand_TextChangeCase(TATEditorCaseConvert.Lower);
    cCommand_TextCaseUpper:            Result:= DoCommand_TextChangeCase(TATEditorCaseConvert.Upper);
    cCommand_TextCaseTitle:            Result:= DoCommand_TextChangeCase(TATEditorCaseConvert.Title);
    cCommand_TextCaseInvert:           Result:= DoCommand_TextChangeCase(TATEditorCaseConvert.Invert);
    cCommand_TextCaseSentence:         Result:= DoCommand_TextChangeCase(TATEditorCaseConvert.Sentence);

    cCommand_TextTrimSpacesLeft:       Result:= DoCommand_TextTrimSpaces(TATTrimSpaces.Left);
    cCommand_TextTrimSpacesRight:      Result:= DoCommand_TextTrimSpaces(TATTrimSpaces.Right);
    cCommand_TextTrimSpacesAll:        Result:= DoCommand_TextTrimSpaces(TATTrimSpaces.All);

    cCommand_FoldAll:                  Result:= DoCommand_FoldAll;
    cCommand_UnfoldAll:                Result:= DoCommand_FoldUnAll;
    cCommand_FoldLevel1..
    cCommand_FoldLevel9:               Result:= DoCommand_FoldLevel(ACmd-cCommand_FoldLevel1);

    cCommand_FoldingFoldAtCurLine:     Result:= DoCommand_FoldRangeAtCurLine(TATEditorFoldRangeCommand.Fold);
    cCommand_FoldingUnfoldAtCurLine:   Result:= DoCommand_FoldRangeAtCurLine(TATEditorFoldRangeCommand.Unfold);
    cCommand_FoldingToggleAtCurLine:   Result:= DoCommand_FoldRangeAtCurLine(TATEditorFoldRangeCommand.Toggle);
    cCommand_FoldingFoldSelection:     Result:= DoCommand_FoldSelection;
    cCommand_FoldingSelectRangeAtCaret: Result:= DoCommand_SelectFoldingRangeAtCaret;

    cCommand_Cancel:                   Result:= DoCommand_Cancel(false, false);
    cCommand_CancelKeepSel:            Result:= DoCommand_Cancel(false, true);
    cCommand_CancelKeepLast:           Result:= DoCommand_Cancel(true, false);
    cCommand_CancelKeepLastAndSel:     Result:= DoCommand_Cancel(true, true);
    cCommand_CancelKeepFirstVisible:   Result:= DoCommand_CancelKeepFirstVisible;

    cCommand_CaretsExtendUpLine:       Result:= DoCommand_CaretsExtend(false, 1);
    cCommand_CaretsExtendUpPage:       Result:= DoCommand_CaretsExtend(false, GetPageLines);
    cCommand_CaretsExtendUpToTop:      Result:= DoCommand_CaretsExtend(false, Strings.Count);
    cCommand_CaretsExtendDownLine:     Result:= DoCommand_CaretsExtend(true, 1);
    cCommand_CaretsExtendDownPage:     Result:= DoCommand_CaretsExtend(true, GetPageLines);
    cCommand_CaretsExtendDownToEnd:    Result:= DoCommand_CaretsExtend(true, Strings.Count);

    cCommand_ZoomIn:                   Result:= DoCommand_ScaleDelta(true);
    cCommand_ZoomOut:                  Result:= DoCommand_ScaleDelta(false);
    cCommand_ZoomReset:                Result:= DoCommand_ScaleReset;
    cCommand_RepeatTextCommand:        DoCommand(FLastTextCmd, TATCommandInvoke.Internal, FLastTextCmdText);
  end;
end;

procedure TATSynEdit.DoCommand(ACmd: integer; AInvoke: TATCommandInvoke; const AText: atString = '');
// AInvoke is used only by CudaText, you may pass any value here
var
  St: TATStrings;
  Caret: TATCaretItem;
  Res: TATCommandResults;
  Str: string;
  bTooManyCarets: boolean;
  NCommandCode: integer;
  i: integer;
begin
  if ACmd<=0 then Exit;
  LastCommandChangedLines:= 0;
  FLastCommandMakesColumnSel:= false;
  DoHotspotsExit;
  St:= Strings;

  if AText<>'' then
    Str:= UTF8Encode(AText)
  else
    Str:= '';

  if AInvoke<>TATCommandInvoke.HotkeyChar then
    FCommandLog.Add(ACmd, AInvoke, Str);

  if DoEventCommand(ACmd, AInvoke, Str) then Exit;

  if IsCommandToUndoInOneStep(ACmd) then
    NCommandCode:= ACmd
  else
    NCommandCode:= 0;
  St.CommandCode:= NCommandCode;

  FCaretSpecPos:= false;

  CalcCaretDistanceFromEdges(ACmd, FCaretDistanceFromTop, FCaretDistanceFromBottom);

  //moving must be with visible caret
  DoCaretForceShow;

  //---handle special command bits (cCmdNNNNN)
  if ACmd and cCmdFlag_KeepSel <> 0 then
    for i:= 0 to Carets.Count-1 do
    begin
      Caret:= Carets[i];
      if Caret.EndY<0 then Caret.EndY:= Caret.PosY;
      if Caret.EndX<0 then Caret.EndX:= Caret.PosX;
    end;

  if ACmd and cCmdFlag_ResetSel <> 0 then
    DoSelect_None;

  if ACmd and cCmdFlag_Caret <> 0 then
  begin
    St.SetGroupMark;
  end;

  //support CudaText #3084
  St.SetNewCommandMark;

  if not FCaretVirtual then
    Carets.UpdateMemory(TATCaretMemoryAction.PrepareX, false);
  FIsRunningCommand:= true;

  bTooManyCarets:= Carets.Count>FOptUndoMaxCarets;
  if bTooManyCarets then
    SetEnabledSlowEvents(false);

  BeginEditing;

  try
    Res:= DoCommandCore(ACmd, AText);
  finally
    EndEditing(TATCommandResult.Text in Res);

    if bTooManyCarets then
      SetEnabledSlowEvents(true);
  end;

  if IsCommandResults_CaretMove(Res) then
    if not FLastCommandMakesColumnSel then
    begin
      ClearSelRectPoints;
    end;

  FLastCommand:= ACmd;
  FLastCommandChangedText2:= FLastCommandChangedText;
  FLastCommandChangedText:= TATCommandResult.Text in Res;
  if FLastCommandChangedText then
  begin
    if ACmd<>cCommand_RepeatTextCommand then
    begin
      FLastTextCmd:= ACmd;
      FLastTextCmdText:= AText;
    end;

    //to solve CudaText issue #3403
    if IsCommandForDelayedParsing(ACmd) then
      StartTimerDelayedParsing;
  end;

  //this logic: to make like in SynWrite, option "Fixed column move".
  //arrows up/down must keep their column, even if sometimes they jump to shorter lines (caret_after_end: off).
  //and special handling for tab-key (for first press of tab-key).
  if not FCaretVirtual then
  case ACmd of
    cCommand_KeyTab:
      begin
        if not FLastCommandChangedText2 then
          Carets.UpdateMemory(TATCaretMemoryAction.SaveX, false);
      end;
    {
    // CudaText issue #1326
    cCommand_ClipboardPaste,
    cCommand_ClipboardPaste_Select,
    cCommand_ClipboardAltPaste,
    cCommand_ClipboardAltAltPaste,
    cCommand_ClipboardAltPaste_Select:
      begin
        Carets.UpdateMemory(TATCaretMemoryAction.SaveX, false);
      end;
    }
    cCommand_KeyUp,
    cCommand_KeyUp_Sel,
    cCommand_KeyDown,
    cCommand_KeyDown_Sel,
    cCommand_KeyPageUp,
    cCommand_KeyPageUp_Sel,
    cCommand_KeyPageDown,
    cCommand_KeyPageDown_Sel:
      Carets.UpdateMemory(TATCaretMemoryAction.SaveX, true);
    else
      Carets.UpdateMemory(TATCaretMemoryAction.ClearX, false);
  end;

  if NCommandCode<>0 then
    St.CommandCode:= 0;

  DoCommandResults(ACmd, Res);
  DoEventCommandAfter(ACmd, AText);
  FIsRunningCommand:= false;
end;


