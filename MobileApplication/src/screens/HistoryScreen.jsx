import React, { useState, useEffect, useMemo, useCallback } from 'react';
import {
  View,
  Text,
  FlatList,
  TextInput,
  TouchableOpacity,
  ActivityIndicator,
  RefreshControl,
  Platform,
  ScrollView,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { SafeAreaView } from 'react-native-safe-area-context';

const COLORS = {
  primary: '#6C63FF',
  background: '#0F0F1A',
  card: '#1A1A2E',
  cardLight: '#222240',
  text: '#FFFFFF',
  textSecondary: '#8A8AA3',
  border: '#2A2A4A',
  success: '#00D9A6',
  danger: '#FF6B8A',
};

export default function HistoryScreen() {
  const [history, setHistory] = useState([]);
  const [isLoading, setIsLoading] = useState(true);
  const [isRefreshing, setIsRefreshing] = useState(false);

  // Filters
  const [searchQuery, setSearchQuery] = useState('');
  const [dateFilter, setDateFilter] = useState('all'); // 'all', 'today', 'week', 'month'
  const [statusFilter, setStatusFilter] = useState('all'); // 'all', 'ACCEPTED', 'REJECTED'

  // Pagination
  const [currentPage, setCurrentPage] = useState(1);
  const itemsPerPage = 5;

  const fetchHistory = async () => {
    try {
      const response = await fetch(`https://broken-unrigged-scolding.ngrok-free.dev/history`, {
        headers: {
          'ngrok-skip-browser-warning': 'true',
        },
      });
      const result = await response.json();
      if (response.ok && result.status === 'success') {
        setHistory(result.history || []);
      }
    } catch (error) {
      console.error('Lỗi lấy lịch sử:', error);
    } finally {
      setIsLoading(false);
      setIsRefreshing(false);
    }
  };

  useEffect(() => {
    fetchHistory();
  }, []);

  const onRefresh = useCallback(() => {
    setIsRefreshing(true);
    fetchHistory();
  }, []);

  // Filter Logic
  const filteredHistory = useMemo(() => {
    let filtered = history;

    // 1. Search by name
    if (searchQuery.trim()) {
      const query = searchQuery.toLowerCase();
      filtered = filtered.filter(item =>
        item.fullname.toLowerCase().includes(query)
      );
    }

    // 2. Filter by status
    if (statusFilter !== 'all') {
      filtered = filtered.filter(item => item.status === statusFilter);
    }

    // 3. Filter by date
    if (dateFilter !== 'all') {
      const now = new Date();
      filtered = filtered.filter(item => {
        // Parse time string "YYYY-MM-DD HH:MM:SS" to Date object
        const itemDate = new Date(item.time.replace(' ', 'T'));

        if (dateFilter === 'today') {
          return itemDate.toDateString() === now.toDateString();
        } else if (dateFilter === 'week') {
          const oneWeekAgo = new Date(now.getTime() - 7 * 24 * 60 * 60 * 1000);
          return itemDate >= oneWeekAgo;
        } else if (dateFilter === 'month') {
          const oneMonthAgo = new Date(now.getTime() - 30 * 24 * 60 * 60 * 1000);
          return itemDate >= oneMonthAgo;
        }
        return true;
      });
    }

    return filtered;
  }, [history, searchQuery, statusFilter, dateFilter]);

  // Reset page to 1 when filters change
  useEffect(() => {
    setCurrentPage(1);
  }, [searchQuery, statusFilter, dateFilter]);

  // Paginated Data
  const totalPages = Math.ceil(filteredHistory.length / itemsPerPage);
  const paginatedHistory = useMemo(() => {
    const startIndex = (currentPage - 1) * itemsPerPage;
    return filteredHistory.slice(startIndex, startIndex + itemsPerPage);
  }, [filteredHistory, currentPage]);

  const renderFilterChips = () => (
    <View className="mb-4">
      <Text className="text-sm font-semibold text-textSecondary mb-2">Thời gian</Text>
      <ScrollView horizontal showsHorizontalScrollIndicator={false} className="mb-4" contentContainerStyle={{ paddingRight: 20 }}>
        {['all', 'today', 'week', 'month'].map((filter) => {
          const labels = { all: 'Tất cả', today: 'Hôm nay', week: 'Tuần này', month: 'Tháng này' };
          const isActive = dateFilter === filter;
          return (
            <TouchableOpacity
              key={filter}
              onPress={() => setDateFilter(filter)}
              className={`mr-2 px-4 py-2 rounded-full border flex-row items-center justify-center ${isActive ? 'bg-primary/20 border-primary' : 'bg-card border-border'}`}
            >
              <Text
                numberOfLines={1}
                className={isActive ? 'text-primary font-bold text-xs' : 'text-textSecondary text-xs'}
              >
                {labels[filter]}
              </Text>
            </TouchableOpacity>
          );
        })}
      </ScrollView>

      <Text className="text-sm font-semibold text-textSecondary mb-2">Trạng thái</Text>
      <View className="flex-row">
        {['all', 'ACCEPTED', 'REJECTED'].map((filter) => {
          const labels = { all: 'Tất cả', ACCEPTED: 'Thành công', REJECTED: 'Thất bại' };
          const colors = { all: COLORS.primary, ACCEPTED: COLORS.success, REJECTED: COLORS.danger };
          const isActive = statusFilter === filter;
          const activeColor = colors[filter];

          return (
            <TouchableOpacity
              key={filter}
              onPress={() => setStatusFilter(filter)}
              className={`mr-2 px-4 py-2 rounded-full border ${isActive ? 'bg-cardLight' : 'bg-card border-border'}`}
              style={isActive ? { borderColor: activeColor, backgroundColor: activeColor + '15' } : {}}
            >
              <Text style={{ color: isActive ? activeColor : COLORS.textSecondary, fontSize: 12, fontWeight: isActive ? '700' : '400' }}>
                {labels[filter]}
              </Text>
            </TouchableOpacity>
          );
        })}
      </View>
    </View>
  );

  const renderHistoryItem = ({ item }) => {
    const isAccepted = item.status === 'ACCEPTED';

    // Tách ngày và giờ để hiển thị đẹp hơn
    const [datePart, timePart] = item.time.split(' ');

    return (
      <View className="bg-card p-4 rounded-[16px] mb-3 border border-border flex-row items-center">
        <View
          className="w-12 h-12 rounded-xl justify-center items-center mr-4"
          style={{ backgroundColor: isAccepted ? COLORS.success + '15' : COLORS.danger + '15' }}
        >
          <Ionicons
            name={isAccepted ? "checkmark-circle" : "close-circle"}
            size={24}
            color={isAccepted ? COLORS.success : COLORS.danger}
          />
        </View>

        <View className="flex-1">
          <Text className="text-base font-bold text-text mb-0.5" numberOfLines={1}>
            {item.fullname}
          </Text>
          <View className="flex-row items-center mt-1">
            <Ionicons name="time-outline" size={12} color={COLORS.textSecondary} />
            <Text className="text-xs text-textSecondary ml-1">{timePart} • {datePart}</Text>
          </View>
        </View>

        <View className={`px-2.5 py-1 rounded-md ${isAccepted ? 'bg-success/20' : 'bg-danger/20'}`} style={{ backgroundColor: isAccepted ? COLORS.success + '20' : COLORS.danger + '20' }}>
          <Text className="text-[10px] font-bold uppercase" style={{ color: isAccepted ? COLORS.success : COLORS.danger }}>
            {isAccepted ? 'MỞ CỬA' : 'TỪ CHỐI'}
          </Text>
        </View>
      </View>
    );
  };

  return (
    <SafeAreaView className="flex-1 bg-background">
      <View className="px-5 pt-4 pb-2">
        <Text className="text-[26px] font-bold text-text tracking-wide mb-4">Lịch sử</Text>

        {/* Search Bar */}
        <View className="flex-row items-center bg-card border border-border rounded-2xl px-4 h-12 mb-4">
          <Ionicons name="search-outline" size={20} color={COLORS.textSecondary} />
          <TextInput
            className="flex-1 text-text ml-2 h-full text-sm"
            placeholder="Tìm kiếm theo tên..."
            placeholderTextColor={COLORS.textSecondary}
            value={searchQuery}
            onChangeText={setSearchQuery}
          />
          {searchQuery !== '' && (
            <TouchableOpacity onPress={() => setSearchQuery('')}>
              <Ionicons name="close-circle" size={18} color={COLORS.textSecondary} />
            </TouchableOpacity>
          )}
        </View>

        {renderFilterChips()}
      </View>

      <View className="flex-1 px-5 pt-2">
        <View className="flex-row justify-between items-end mb-3">
          <Text className="text-sm font-semibold text-textSecondary">
            Hiển thị {filteredHistory.length} kết quả
          </Text>
        </View>

        {isLoading ? (
          <View className="flex-1 justify-center items-center">
            <ActivityIndicator size="large" color={COLORS.primary} />
          </View>
        ) : (
          <>
            <FlatList
              data={paginatedHistory}
              keyExtractor={(item, index) => index.toString()}
              renderItem={renderHistoryItem}
              showsVerticalScrollIndicator={false}
              contentContainerStyle={{ paddingBottom: 10 }}
              refreshControl={
                <RefreshControl
                  refreshing={isRefreshing}
                  onRefresh={onRefresh}
                  tintColor={COLORS.primary}
                  colors={[COLORS.primary]}
                />
              }
              ListEmptyComponent={() => (
                <View className="items-center justify-center py-10 mt-10">
                  <Ionicons name="document-text-outline" size={48} color={COLORS.border} />
                  <Text className="text-textSecondary mt-4 text-center">
                    Không tìm thấy lịch sử mở cửa nào phù hợp.
                  </Text>
                </View>
              )}
            />

            {/* Pagination Controls */}
            {totalPages > 1 && (
              <View className="flex-row justify-between items-center pt-3 pb-5 border-t border-border mt-2">
                <TouchableOpacity
                  onPress={() => setCurrentPage(prev => Math.max(1, prev - 1))}
                  disabled={currentPage === 1}
                  className={`px-4 py-2.5 rounded-xl justify-center items-center ${currentPage === 1 ? 'opacity-40 bg-card border border-border' : 'bg-card border border-border'}`}
                >
                  <Text className="text-text font-semibold text-sm">Trước</Text>
                </TouchableOpacity>

                <Text className="text-textSecondary text-sm font-semibold">
                  Trang <Text className="text-primary">{currentPage}</Text> / {totalPages}
                </Text>

                <TouchableOpacity
                  onPress={() => setCurrentPage(prev => Math.min(totalPages, prev + 1))}
                  disabled={currentPage === totalPages}
                  className={`px-4 py-2.5 rounded-xl justify-center items-center ${currentPage === totalPages ? 'opacity-40 bg-card border border-border' : 'bg-primary'}`}
                >
                  <Text className={currentPage === totalPages ? "text-text font-semibold text-sm" : "text-white font-semibold text-sm"}>Sau</Text>
                </TouchableOpacity>
              </View>
            )}
          </>
        )}
      </View>
    </SafeAreaView>
  );
}
