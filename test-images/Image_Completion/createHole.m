
for i=1
   
    if exist(strcat('test',num2str(i),'.bmp'), 'file')==2
        
        a = imread(strcat('test',num2str(i),'.bmp'));
        mask = ~logical(imread(strcat('test',num2str(i),'m.bmp')));
        r = a.*uint8(~mask);
        imwrite(mask(:,:,1), strcat('test',num2str(i),'m.bmp'));
        imwrite(r, strcat('test',num2str(i),'h.bmp'));
    else
        a = imread(strcat('test',num2str(i),'.jpg'));
        mask = ~logical(imread(strcat('test',num2str(i),'m.jpg')));
        r = a.*uint8(~mask);
        imwrite(mask(:,:,1), strcat('test',num2str(i),'m.jpg'));
        imwrite(r, strcat('test',num2str(i),'h.jpg'));
    end
    
end