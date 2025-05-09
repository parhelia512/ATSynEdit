{$ifdef nnn}begin end;{$endif}

function TATSynEdit.DoCommand_ToggleOverwrite: TATCommandResults;
begin
  ModeOverwrite:= not ModeOverwrite;
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleWordWrap(AltOrder: boolean): TATCommandResults;
var
  bCaretVisible: boolean;
begin
  bCaretVisible:= IsCaretOnVisibleRect;

  case FWrapMode of
    TATEditorWrapMode.ModeOff:
      OptWrapMode:= TATEditorWrapMode.ModeOn;
    TATEditorWrapMode.ModeOn:
      begin
        if AltOrder then
          OptWrapMode:= TATEditorWrapMode.AtWindowOrMargin
        else
          OptWrapMode:= TATEditorWrapMode.ModeOff;
      end;
    TATEditorWrapMode.AtWindowOrMargin:
      OptWrapMode:= TATEditorWrapMode.ModeOff;
  end;

  Include(ModifiedOptions, TATEditorModifiedOption.WordWrap);
  Result:= [TATCommandResult.State];
  if bCaretVisible then
    Include(Result, TATCommandResult.CaretTop);
end;

function TATSynEdit.DoCommand_ToggleUnprinted: TATCommandResults;
begin
  OptUnprintedVisible:= not OptUnprintedVisible;
  Include(ModifiedOptions, TATEditorModifiedOption.UnprintedVisible);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleUnprintedSpaces: TATCommandResults;
begin
  OptUnprintedSpaces:= not OptUnprintedSpaces;
  Include(ModifiedOptions, TATEditorModifiedOption.UnprintedSpaces);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleUnprintedSpacesTrailing: TATCommandResults;
begin
  OptUnprintedSpacesTrailing:= not OptUnprintedSpacesTrailing;
  Include(ModifiedOptions, TATEditorModifiedOption.UnprintedTrailingOnly);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleUnprintedEnds: TATCommandResults;
begin
  OptUnprintedEnds:= not OptUnprintedEnds;
  Include(ModifiedOptions, TATEditorModifiedOption.UnprintedEnds);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleUnprintedEndDetails: TATCommandResults;
begin
  OptUnprintedEndsDetails:= not OptUnprintedEndsDetails;
  Include(ModifiedOptions, TATEditorModifiedOption.UnprintedEndDetails);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleLineNums: TATCommandResults;
begin
  with Gutter.Items[FGutterBandNumbers] do
    Visible:= not Visible;
  Include(ModifiedOptions, TATEditorModifiedOption.GutterNumbers);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleFolding: TATCommandResults;
begin
  with Gutter.Items[FGutterBandFolding] do
    Visible:= not Visible;
  Include(ModifiedOptions, TATEditorModifiedOption.GutterFolding);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleRuler: TATCommandResults;
begin
  OptRulerVisible:= not OptRulerVisible;
  Include(ModifiedOptions, TATEditorModifiedOption.RulerVisible);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleMiniMap: TATCommandResults;
begin
  OptMinimapVisible:= not OptMinimapVisible;
  Include(ModifiedOptions, TATEditorModifiedOption.MinimapVisible);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleMicroMap: TATCommandResults;
begin
  OptMicromapVisible:= not OptMicromapVisible;
  Include(ModifiedOptions, TATEditorModifiedOption.MicromapVisible);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ToggleReadOnly: TATCommandResults;
begin
  ModeReadOnly:= not ModeReadOnly;
  Include(ModifiedOptions, TATEditorModifiedOption.ReadOnly);
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_CaretsExtend(ADown: boolean; ALines: integer): TATCommandResults;
begin
  DoCaretsExtend(ADown, ALines);
  if ADown then
    Result:= [TATCommandResult.CaretBottom]
  else
    Result:= [TATCommandResult.CaretTop];
end;

function TATSynEdit.DoCommand_ScaleDelta(AIncrease: boolean): TATCommandResults;
begin
  DoScaleFontDelta(AIncrease, true);
  DoEventZoom;
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_ScaleReset: TATCommandResults;
begin
  //use OptScaleFont setter, not FOptScaleFont
  OptScaleFont:= 0;
  DoEventZoom;
  Result:= [TATCommandResult.State];
end;

function TATSynEdit.DoCommand_FoldAll: TATCommandResults;
var
  R: PATFoldRange;
  NeedFixCaret: boolean;
  i: integer;
begin
  for i:= 0 to Fold.Count-1 do
  begin
    R:= Fold.ItemPtr(i);
    if R^.IsSimple then Continue;
    DoRangeFold(i);
  end;

  NeedFixCaret:= false;
  for i:= 0 to Carets.Count-1 do
    if IsLineFolded(Carets[i].PosY) then
    begin
      NeedFixCaret:= true;
      Break;
    end;
  if NeedFixCaret then
    DoCaretSingle(0, 0);

  Result:= [TATCommandResult.CaretTop, TATCommandResult.Scroll, TATCommandResult.FoldChange];
end;

procedure TATSynEdit.DoMarkAllRangesUnfolded;
var
  R: PATFoldRange;
  i: integer;
begin
  for i:= 0 to Fold.Count-1 do
  begin
    R:= Fold.ItemPtr(i);
    if R^.Folded then
    begin
      R^.Folded:= false;
      FWrapUpdateNeeded:= true;
    end;
  end;
end;

procedure TATSynEdit.DoMarkLinesVisible(ALineFrom, ALineTo: integer);
var
  St: TATStrings;
  i: integer;
begin
  St:= Strings;
  ALineTo:= Min(ALineTo, St.Count-1);

  for i:= ALineFrom to ALineTo do
  begin
    St.LinesHidden[i, EditorIndex]:= false;
    St.LinesFoldFrom[i, EditorIndex]:= 0;
  end;
end;

function TATSynEdit.DoCommand_FoldUnAll: TATCommandResults;
begin
  DoMarkAllRangesUnfolded;
  DoMarkLinesVisible(0, MaxInt);

  Result:= [TATCommandResult.CaretTop, TATCommandResult.Scroll, TATCommandResult.FoldChange];
end;

function TATSynEdit.DoCommand_FoldLevel(ALevel: integer): TATCommandResults;
begin
  DoCommand_FoldUnAll;
  DoFoldForLevelEx(ALevel, -1);
  Result:= [TATCommandResult.CaretAny, TATCommandResult.Scroll, TATCommandResult.FoldChange];
end;


function TATSynEdit.DoCommand_FoldRangeAtCurLine(ACommand: TATEditorFoldRangeCommand): TATCommandResults;
begin
  Result:= [];
  //if Carets.Count<>1 then Exit; //we don't need this cmd for multi-carets
  if DoFoldUnfoldRangeAtCurLine(ACommand) then
    Result:= [TATCommandResult.CaretAny, TATCommandResult.Scroll, TATCommandResult.FoldChange];
end;


function TATSynEdit.DoCommand_FoldSelection: TATCommandResults;
var
  Caret: TATCaretItem;
  Y1, Y2, NRangeIndex: integer;
begin
  Result:= [];
  if Carets.Count<>1 then exit;
  Caret:= Carets[0];
  if not Caret.IsSelection then exit;
  Caret.GetSelLines(Y1, Y2);

  //better clear the selection
  Caret.SelectNone;

  //don't use FindRangeWithPlusAtLine because it uses binary search, while
  //range 'for selection' is added unsorted
  if Fold.FindRangeWithPlusAtLine_ViaIndexer(Y1)>=0 then exit;

  Fold.AddSorted(1, Y1, 1, Y2, false, '', cTagPersistentFoldRange, NRangeIndex);
  DoRangeFold(NRangeIndex);

  Fold.ClearLineIndexer(Strings.Count, true);
  Fold.UpdateLineIndexer;

  Result:= [TATCommandResult.CaretAny, TATCommandResult.FoldChange];
end;

