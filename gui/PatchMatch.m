function varargout = PatchMatch(varargin)
% PATCHMATCH MATLAB code for PatchMatch.fig
%      PATCHMATCH, by itself, creates a new PATCHMATCH or raises the existing
%      singleton*.
%
%      H = PATCHMATCH returns the handle to a new PATCHMATCH or the handle to
%      the existing singleton*.
%
%      PATCHMATCH('CALLBACK',hObject,eventData,handles,...) calls the local
%      function named CALLBACK in PATCHMATCH.M with the given input arguments.
%
%      PATCHMATCH('Property','Value',...) creates a new PATCHMATCH or raises the
%      existing singleton*.  Starting from the left, property value pairs are
%      applied to the GUI before PatchMatch_OpeningFcn gets called.  An
%      unrecognized property name or invalid value makes property application
%      stop.  All inputs are passed to PatchMatch_OpeningFcn via varargin.
%
%      *See GUI Options on GUIDE's Tools menu.  Choose "GUI allows only one
%      instance to run (singleton)".
%
% See also: GUIDE, GUIDATA, GUIHANDLES

% Edit the above text to modify the response to help PatchMatch

% Last Modified by GUIDE v2.5 04-Apr-2018 00:26:10

% Begin initialization code - DO NOT EDIT


gui_Singleton = 1;
gui_State = struct('gui_Name',       mfilename, ...
                   'gui_Singleton',  gui_Singleton, ...
                   'gui_OpeningFcn', @PatchMatch_OpeningFcn, ...
                   'gui_OutputFcn',  @PatchMatch_OutputFcn, ...
                   'gui_LayoutFcn',  [] , ...
                   'gui_Callback',   []);
if nargin && ischar(varargin{1})
    gui_State.gui_Callback = str2func(varargin{1});
end

if nargout
    [varargout{1:nargout}] = gui_mainfcn(gui_State, varargin{:});
else
    gui_mainfcn(gui_State, varargin{:});
end
% End initialization code - DO NOT EDIT


% --- Executes just before PatchMatch is made visible.
function PatchMatch_OpeningFcn(hObject, eventdata, handles, varargin)
% This function has no output args, see OutputFcn.
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
% varargin   command line arguments to PatchMatch (see VARARGIN)
global img;
global msk;
global mskcol;
global wbmFlg;
global point0;
global drawFlg;
global pnSize;
global imSize;
global viewMode;

img = [];
msk = [];
mskcol = [0 0 0];
wbmFlg = 0;
point0 = [];
drawFlg = -1;
imSize = [];
pnSize = 5;
viewMode = 1;
% Choose default command line output for PatchMatch
handles.output = hObject;

% Update handles structure
guidata(hObject, handles);

%set(hObject,... 
%'WindowButtonMotionFcn',{@uipanel1_ButtonMotionFcn},...
%'WindowButtonUpFcn',{@uipanel1_ButtonUpFcn},...
%'WindowButtonDownFcn',{@uipanel1_ButtonDownFcn});

% UIWAIT makes PatchMatch wait for user response (see UIRESUME)
% uiwait(handles.figure1);


% --- Outputs from this function are returned to the command line.
function varargout = PatchMatch_OutputFcn(hObject, eventdata, handles) 
% varargout  cell array for returning output args (see VARARGOUT);
% hObject    handle to figure
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Get default command line output from handles structure
varargout{1} = handles.output;


function uipanel1_ButtonDownFcn(hObject, eventdata, handles)
% hObject    handle to figure1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% --- Executes on button press in pushbutton1.
function pushbutton1_Callback(hObject, eventdata, handles)
% hObject    handle to pushbutton1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

global filename;
global dir;
[filename, dir] = uigetfile( '*.*', 'Load Image');

global img;
global msk;
global imSize;

img = imread([dir filename]);
imSize = size(img);
msk = zeros(imSize(1),imSize(2),'uint8');
axes(handles.uipanel1);
imshow(img);




function edit1_Callback(hObject, eventdata, handles)
% hObject    handle to edit1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)

% Hints: get(hObject,'String') returns contents of edit1 as text
%        str2double(get(hObject,'String')) returns contents of edit1 as a double


% --- Executes during object creation, after setting all properties.
function edit1_CreateFcn(hObject, eventdata, handles)
% hObject    handle to edit1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called

% Hint: edit controls usually have a white background on Windows.
%       See ISPC and COMPUTER.
if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end


% --- Executes during object creation, after setting all properties.
function pushbutton1_CreateFcn(hObject, eventdata, handles)
% hObject    handle to pushbutton1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    empty - handles not created until after all CreateFcns called


% --- Executes on mouse press over figure background, over a disabled or
% --- inactive control, or over an axes background.
function figure1_WindowButtonDownFcn(hObject, eventdata, handles)
% hObject    handle to figure1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
global drawFlg;
global point0;

mouse = get(gcf,'SelectionType');
if( strcmpi( mouse, 'normal' ) )
    drawFlg = 1;
    point0 = getPixelPosition();

elseif( strcmpi( mouse, 'alt' ) )
    drawFlg = 0;
    point0 = getPixelPosition();

else
    drawFlg = -1;
end


% --- Executes on mouse motion over figure - except title and menu.
function figure1_WindowButtonMotionFcn(hObject, eventdata, handles)
% hObject    handle to figure1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
global pnSize;
global imSize;
global msk;
global point0;
global drawFlg;

global wbmFlg;

if( wbmFlg == 0 && drawFlg >= 0 )    
    wbmFlg = 1;
    
    point1 = getPixelPosition()
    
    if( ~isempty( point0 ) )
        ps = pnSize / 480 * max([imSize(1),imSize(2)]);
        msk = drawLine( msk, point0, point1, ps, drawFlg );
        showImageMask(handles.uipanel1); 
    end
    
    point0 = point1;
    
    wbmFlg = 0;
end


% --- Executes on mouse press over figure background, over a disabled or
% --- inactive control, or over an axes background.
function figure1_WindowButtonUpFcn(hObject, eventdata, handles)
% hObject    handle to figure1 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
global drawFlg;
global point0;
drawFlg = -1;
point0 = [];

function point = getPixelPosition()
global imSize;

if( isempty( imSize ) )
    point = [];
else
    cp = get (gca, 'CurrentPoint');
    cp = cp(1,1:2);
    row = int32(round( axes2pix(imSize(1), [1 imSize(1)], cp(2)) ));
    col = int32(round( axes2pix(imSize(2), [1 imSize(2)], cp(1)) ));

    point = [row col];
end


% --- Executes on button press in pushbutton2.
function pushbutton2_Callback(hObject, eventdata, handles)
% hObject    handle to pushbutton2 (see GCBO)
% eventdata  reserved - to be defined in a future version of MATLAB
% handles    structure with handles and user data (see GUIDATA)
global filename;
global dir;
global img;
global msk;
global mskcol;

[d name ext] = fileparts(filename);

msk = uint8(msk*255);
maskimage = double(img).*double(repmat(~msk,1,1,3));
imwrite( uint8(maskimage), [dir name '_img.png'] );
imwrite( uint8(msk), [dir name '_msk.png'] );

function [] = showImageMask(handle)
global img;
global msk;
global mskcol;
global viewMode;

if( ~isempty(img) )
    switch( viewMode )
        case 1
            maskimage = genImageMask( single(img), single(msk), single(mskcol) );
        case 2
            maskimage = reshape( mskcol, [1 1 3] );
            maskimage = single(repmat( maskimage, [size(msk,1), size(msk,2), 1] )) .* single(repmat( msk, [1 1 3] ));            
        case 3
            maskimage = img;
        otherwise
            maskimage = zeros(size(msk));
    end
    axes(handle);
    imshow(uint8(maskimage));
end
