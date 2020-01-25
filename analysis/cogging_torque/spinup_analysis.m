load spinup_2

setcur = data(1,:);
t = data(2,:);
pos = data(3,:);
cur = data(4,:);

% plot raw data
figure(1);clf;
for i = 1:length(setcur)
    subplot(2, fix(length(setcur)/2), fix((i-1)/2)+1)
    plot(t{i}, pos{i})
    hold on
    subplot(2, fix(length(setcur)/2), fix((i-1)/2)+1+fix(length(setcur)/2))
    plot(t{i}, cur{i})
    hold on
end
% axes[0, 0].set_ylabel('Pos')
% axes[1, 0].set_ylabel('Cur')
% suptitle('Ramp raw data')

%%
% plot acc vs cur
figure(2);clf;
for i = 1:length(setcur)-1
    t{i} = smooth(t{i}, 105, 'sgolay')';
    pos2{i} = smooth(pos{i}, 105, 'sgolay')' / 8192;
    cur{i} = smooth(cur{i}, 15, 'sgolay')';
    acc = gradient(gradient(pos2{i}) ./ gradient(t{i})) ./ gradient(t{i});
    P = polyfit(t{i}, pos{i} / 8192, 2);
    a = 2*P(1);
    toplot = [cur{i}', acc'];
    toplot = rmoutliers(toplot);
    toplot = mean(toplot);
%     figure(2);
%     plot(toplot(:,1), toplot(:,2), 'k*',...
%          'DisplayName', sprintf('I=%.1f',setcur{i}))
    hold on
    plot(setcur{i} * -(-1).^i, a, 'ro',...
         'DisplayName', sprintf('I=%.1f',setcur{i}))
%     figure(3);
%     plot(toplot(:,2));
%     hold on;
end

ivals = linspace(-setcur{end}, setcur{end});
% I = 1.00E-04; % kg.m^2
I = 2.5E-4;
Tvals = ivals / 150 * 8.269933431; % Nm
avals = Tvals / I / 2 / pi;
plot(ivals, avals, 'k--');
xlabel('Current (A)');
ylabel('Angular acceleration (r/s^2)');

% legend show
grid on
title('Torque vs Current');